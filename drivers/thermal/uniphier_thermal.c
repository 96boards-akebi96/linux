/**
 * uniphier_thermal.c - Socionext UniPhier thermal driver
 *
 * Copyright 2014      Panasonic Corporation
 * Copyright 2016-2017 Socionext Inc.
 * All rights reserved.
 *
 * Author:
 *	Kunihiko Hayashi <hayashi.kunihiko@socionext.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>

#include "thermal_core.h"

/*
 * block registers
 * addresses are the offset from .block_base
 */
#define PVTCTLEN			0x0000
#define PVTCTLEN_EN			BIT(0)

#define PVTCTLMODE			0x0004
#define PVTCTLMODE_MASK			0xf
#define PVTCTLMODE_TEMPMON		0x5

#define EMONREPEAT			0x0040
#define EMONREPEAT_ENDLESS		BIT(24)
#define EMONREPEAT_PERIOD		GENMASK(3, 0)
#define EMONREPEAT_PERIOD_1000000	0x9

/*
 * common registers
 * addresses are the offset from .map_base
 */
#define PVTCTLSEL			0x0900
#define PVTCTLSEL_MASK			GENMASK(2, 0)
#define PVTCTLSEL_MONITOR		0

#define SETALERT0			0x0910
#define SETALERT1			0x0914
#define SETALERT2			0x0918
#define SETALERT_TEMP_OVF		(GENMASK(7, 0) << 16)
#define SETALERT_TEMP_OVF_VALUE(val)	(((val) & GENMASK(7, 0)) << 16)
#define SETALERT_EN			BIT(0)

#define PMALERTINTCTL			0x0920
#define PMALERTINTCTL_CLR(ch)		BIT(4 * (ch) + 2)
#define PMALERTINTCTL_SET(ch)		BIT(4 * (ch) + 1)
#define PMALERTINTCTL_EN(ch)		BIT(4 * (ch) + 0)
#define PMALERTINTCTL_MASK		(GENMASK(10, 8) | GENMASK(6, 4) | \
					 GENMASK(2, 0))

#define TMOD				0x0928
#define TMOD_WIDTH			9

#define TMODCOEF			0x0e5c

#define TMODSETUP0_EN			BIT(30)
#define TMODSETUP0_VAL(val)		(((val) & GENMASK(13, 0)) << 16)
#define TMODSETUP1_EN			BIT(15)
#define TMODSETUP1_VAL(val)		((val) & GENMASK(14, 0))

/* SoC critical temperature */
#define CRITICAL_TEMP_LIMIT		(120 * 1000)

/* Max # of alert channels */
#define ALERT_CH_NUM			3

/* SoC specific thermal sensor data */
struct uniphier_tm_soc_data {
	u32 block_base;
	u32 tmod_setup_addr;
	u32 tmod_calib[2];
};

struct uniphier_tm_dev {
	struct device *dev;
	void __iomem  *base;
	bool alert_en[ALERT_CH_NUM];
	struct thermal_zone_device *tz_dev;
	const struct uniphier_tm_soc_data *data;
};

static void maskwritel(void __iomem *base, u32 offset, u32 mask, u32 val)
{
	u32 tmp = readl_relaxed(base + offset);

	tmp &= mask;
	tmp |= val & mask;
	writel_relaxed(val, base + offset);
}

static u32 maskreadl(void __iomem *base, u32 offset, u32 mask)
{
	return (readl_relaxed(base + offset) & mask);
}

static void uniphier_tm_initialize_sensor(struct uniphier_tm_dev *tdev)
{
	void __iomem *base = tdev->base;
	u32 val;

	/* stop PVT */
	maskwritel(base, tdev->data->block_base + PVTCTLEN,
		    PVTCTLEN_EN, 0);

	/*
	 * Since SoC has a calibrated value that was set in advance,
	 * TMODCOEF shows non-zero and PVT refers the value internally.
	 *
	 * If TMODCOEF shows zero, the boards don't have the calibrated
	 * value, and the driver has to set default value from SoC data.
	 */
	val = readl(base + TMODCOEF);
	if (!val)
		maskwritel(base, tdev->data->tmod_setup_addr, ((u32)~0),
			   TMODSETUP0_EN |
			   TMODSETUP0_VAL(tdev->data->tmod_calib[0]) |
			   TMODSETUP1_EN |
			   TMODSETUP1_VAL(tdev->data->tmod_calib[1]));

	/* select temperature mode */
	maskwritel(base, tdev->data->block_base + PVTCTLMODE,
		   PVTCTLMODE_MASK, PVTCTLMODE_TEMPMON);

	/* set monitoring period */
	maskwritel(base, tdev->data->block_base + EMONREPEAT,
		   EMONREPEAT_ENDLESS | EMONREPEAT_PERIOD,
		   EMONREPEAT_ENDLESS | EMONREPEAT_PERIOD_1000000);

	/* set monitor mode */
	maskwritel(base, PVTCTLSEL, PVTCTLSEL_MASK, PVTCTLSEL_MONITOR);
}

static void uniphier_tm_set_alert(struct uniphier_tm_dev *tdev, u32 ch,
				  u32 temp)
{
	void __iomem *base = tdev->base;

	/* set alert temperature */
	maskwritel(base, SETALERT0 + (ch << 2),
		   SETALERT_EN | SETALERT_TEMP_OVF,
		   SETALERT_EN |
		   SETALERT_TEMP_OVF_VALUE(temp / 1000));
}

static void uniphier_tm_enable_sensor(struct uniphier_tm_dev *tdev)
{
	void __iomem *base = tdev->base;
	int i;
	u32 bits = 0;

	for (i = 0; i < ALERT_CH_NUM; i++)
		if (tdev->alert_en[i])
			bits |= PMALERTINTCTL_EN(i);

	/* enable alert interrupt */
	maskwritel(base, PMALERTINTCTL, PMALERTINTCTL_MASK, bits);

	/* start PVT */
	maskwritel(base, tdev->data->block_base + PVTCTLEN,
		    PVTCTLEN_EN, PVTCTLEN_EN);

	usleep_range(700, 1500);	/* The spec note says at least 700us */
}

static void uniphier_tm_disable_sensor(struct uniphier_tm_dev *tdev)
{
	void __iomem *base = tdev->base;

	/* disable alert interrupt */
	maskwritel(base, PMALERTINTCTL, PMALERTINTCTL_MASK, 0);

	/* stop PVT */
	maskwritel(base, tdev->data->block_base + PVTCTLEN,
		   PVTCTLEN_EN, 0);

	usleep_range(1000, 2000);	/* The spec note says at least 1ms */
}

static int uniphier_tm_get_temp(void *data, int *out_temp)
{
	struct uniphier_tm_dev *tdev = data;
	u32 temp;

	temp = maskreadl(tdev->base, TMOD, GENMASK(TMOD_WIDTH - 1, 0));

	/* MSB of the TMOD field is a sign bit */
	*out_temp = sign_extend32(temp, TMOD_WIDTH - 1) * 1000;

	return 0;
}

static const struct thermal_zone_of_device_ops uniphier_of_thermal_ops = {
	.get_temp = uniphier_tm_get_temp,
};

static void uniphier_tm_irq_clear(struct uniphier_tm_dev *tdev)
{
	u32 mask = 0, bits = 0;
	int i;

	for (i = 0; i < ALERT_CH_NUM; i++) {
		mask |= (PMALERTINTCTL_CLR(i) | PMALERTINTCTL_SET(i));
		bits |= PMALERTINTCTL_CLR(i);
	}

	/* clear alert interrupt */
	maskwritel(tdev->base, PMALERTINTCTL, mask, bits);
}

static irqreturn_t uniphier_tm_alarm_irq(int irq, void *_tdev)
{
	struct uniphier_tm_dev *tdev = _tdev;

	disable_irq_nosync(irq);
	uniphier_tm_irq_clear(tdev);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t uniphier_tm_alarm_irq_thread(int irq, void *_tdev)
{
	struct uniphier_tm_dev *tdev = _tdev;

	thermal_zone_device_update(tdev->tz_dev);

	return IRQ_HANDLED;
}

static const struct of_device_id uniphier_tm_dt_ids[];

static int uniphier_tm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct uniphier_tm_dev *tdev;
	const struct thermal_trip *trips;
	int i, ret, irq, ntrips, crit_temp = INT_MAX;

	tdev = devm_kzalloc(dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;
	tdev->dev = dev;

	tdev->data = of_device_get_match_data(dev);
	if (WARN_ON(!tdev->data))
		return -EINVAL;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tdev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(tdev->base)) {
		dev_err(dev, "failed to map resource\n");
		return PTR_ERR(tdev->base);
	}

	uniphier_tm_initialize_sensor(tdev);
	if (ret) {
		dev_err(dev, "failed to initialize sensor\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, irq, uniphier_tm_alarm_irq,
					uniphier_tm_alarm_irq_thread,
					0, "thermal", tdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, tdev);

	tdev->tz_dev = thermal_zone_of_sensor_register(dev, 0, tdev,
						&uniphier_of_thermal_ops);
	if (IS_ERR(tdev->tz_dev)) {
		dev_err(dev, "failed to register sensor device\n");
		return PTR_ERR(tdev->tz_dev);
	}

	/* get trip points */
	trips = of_thermal_get_trip_points(tdev->tz_dev);
	ntrips = of_thermal_get_ntrips(tdev->tz_dev);
	if (ntrips > ALERT_CH_NUM) {
		dev_err(dev, "thermal zone has too many trips\n");
		ret = -E2BIG;
		goto err_sensor;
	}

	/* set alert temperatures */
	for (i = 0; i < ntrips; i++) {
		if (trips[i].type == THERMAL_TRIP_CRITICAL &&
		    trips[i].temperature < crit_temp)
			crit_temp = trips[i].temperature;
		uniphier_tm_set_alert(tdev, i, trips[i].temperature);
		tdev->alert_en[i] = true;
	}
	if (crit_temp > CRITICAL_TEMP_LIMIT) {
		dev_err(dev, "critical trip is over limit(>%d), or not set\n",
			CRITICAL_TEMP_LIMIT);
		ret = -EINVAL;
		goto err_sensor;
	}

	uniphier_tm_enable_sensor(tdev);

	dev_info(dev, "thermal driver (limit=%d C)\n", crit_temp / 1000);

	return 0;

err_sensor:
	thermal_zone_of_sensor_unregister(dev, tdev->tz_dev);

	return ret;
}

static int uniphier_tm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct uniphier_tm_dev *tdev = platform_get_drvdata(pdev);

	/* disable sensor */
	uniphier_tm_disable_sensor(tdev);
	thermal_zone_of_sensor_unregister(dev, tdev->tz_dev);

	return 0;
}

static const struct uniphier_tm_soc_data uniphier_pxs2_tm_data = {
	.block_base      = 0x0000,
	.tmod_setup_addr = 0x0904,
	.tmod_calib      = {0x4f86, 0xe844},
};

static const struct uniphier_tm_soc_data uniphier_ld20_tm_data = {
	.block_base      = 0x0800,
	.tmod_setup_addr = 0x0938,
	.tmod_calib      = {0x4f22, 0xe8ee},
};

static const struct of_device_id uniphier_tm_dt_ids[] = {
	{
		.compatible = "socionext,proxstream2-thermal",
		.data       = &uniphier_pxs2_tm_data,
	},
	{
		.compatible = "socionext,ph1-ld20-thermal",
		.data       = &uniphier_ld20_tm_data,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, uniphier_tm_dt_ids);

static struct platform_driver uniphier_tm_driver = {
	.probe = uniphier_tm_probe,
	.remove = uniphier_tm_remove,
	.driver = {
		.name = "uniphier-thermal",
		.of_match_table = uniphier_tm_dt_ids,
	},
};
module_platform_driver(uniphier_tm_driver);

MODULE_AUTHOR("Kunihiko Hayashi <hayashi.kunihiko@socionext.com>");
MODULE_DESCRIPTION("UniPhier thermal driver");
MODULE_LICENSE("GPL v2");
