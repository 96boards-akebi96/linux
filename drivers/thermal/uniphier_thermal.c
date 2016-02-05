/**
 * uniphier_thermal.c - Socionext Uniphier thermal management driver
 *
 * Copyright (c) 2014 Panasonic Corporation
 * Copyright (c) 2016 Socionext Inc.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <linux/thermal.h>
#include <linux/interrupt.h>

#include "thermal_core.h"

#define PMPVTCTLENA_OFFSET			0x00000000
#define PMPVTCTLENA_PMPVTCTLEN_A		(0x1 << 0)
#define PMPVTCTLENA_PMPVTCTLEN_A_STOP		(0x0 << 0)
#define PMPVTCTLENA_PMPVTCTLEN_A_START		(0x1 << 0)

#define PMPVTCTLMODEA_OFFSET			0x00000004
#define PMPVTCTLMODEA_PMPVTCTLMODE_A		(0xf << 0)
#define PMPVTCTLMODEA_PMPVTCTLMODE_A_TEMPMON	(0x5 << 0)

#define TCOUNTA_OFFSET				0x00000018
#define TCOUNTA_V_TCOUNT_A			(0x3ff << 0)

#define EMONREPEATA_OFFSET			0x00000040
#define EMONREPEATA_EMONENDLESS_A		(0x1 << 24)
#define EMONREPEATA_EMONENDLESS_A_DISABLE	(0x0 << 24)
#define EMONREPEATA_EMONENDLESS_A_ENABLE	(0x1 << 24)
#define EMONREPEATA_EMONNUM_A			(0xfff << 8)
#define EMONREPEATA_EMONNUM_A_1TIME		(0x1 << 8)
#define EMONREPEATA_EMONPERIOD_A		(0xf << 0)
#define EMONREPEATA_EMONPERIOD_A_1000000	(0x9 << 0)

#define PMPVTCTLMODESEL_OFFSET			0x00000900
#define PMPVTCTLMODESEL_PMPVTCTLMODESEL		(0x7 << 0)
#define PMPVTCTLMODESEL_PMPVTCTLMODESEL_MAIN	(0x1 << 0)

#define SETFANPARAM1_OFFSET			0x00000904
#define SETFANPARAM1_ETMODINTERCEPTSEL		(0x1 << 30)
#define SETFANPARAM1_ETMODINTERCEPTSEL_EFUSE	(0x0 << 30)
#define SETFANPARAM1_ETMODINTERCEPTSEL_ETMODINTERCEPT \
						(0x1 << 30)
#define SETFANPARAM1_ETMODINTERCEPT		(0x3fff << 16)
#define SETFANPARAM1_ETMODSLOPESEL		(0x1 << 15)
#define SETFANPARAM1_ETMODSLOPESEL_EFUSE	(0x0 << 15)
#define SETFANPARAM1_ETMODSLOPESEL_ETMODSLOPE	(0x1 << 15)
#define SETFANPARAM1_ETMODSLOPE			(0x7fff << 0)

#define SETALERT0_OFFSET			0x00000910
#define SETALERT1_OFFSET			0x00000914
#define SETALERT2_OFFSET			0x00000918
#define SETALERT_EALERTTEMP0			(0xff << 8)
#define SETALERT_EOFUFSEL0			(0x1 << 4)
#define SETALERT_EOFUFSEL0_HIGHER		(0x0 << 4)
#define SETALERT_EOFUFSEL0_LOWER		(0x1 << 4)
#define SETALERT_EALERTEN0			(0x1 << 0)
#define SETALERT_EALERTEN0_DISUSE		(0x0 << 0)
#define SETALERT_EALERTEN0_USE			(0x1 << 0)

#define PMALERTINTCTL_OFFSET			0x00000920
#define PMALERTINTCTL_ALERTINT_CLR(ch)		(0x1 << (((ch)<<4) + 2))
#define PMALERTINTCTL_ALERTINT_ST(ch)		(0x1 << (((ch)<<4) + 1))
#define PMALERTINTCTL_ALERTINT_EN(ch)		(0x1 << (((ch)<<4) + 0))
#define PMALERTINTCTL_ALL_BITS			(0x777 << 0)

#define TMOD_OFFSET				0x00000928
#define TMOD_V_TMOD				(0x1ff << 0)

#define TMODCOEF_OFFSET				0x00000e5c
#define TMODCOEF_V_TMODINTERCEPT		(0x3fff << 16)
#define TMODCOEF_V_TMODSLOPE			(0x7fff << 0)

#define SETFANPARAM1_ETMODINTERCEPT_SETTINGS	(0x0f86 << 16)
#define SETFANPARAM1_ETMODSLOPE_SETTINGS	(0x6844 << 0)

#define TEMP_LIMIT_DEFAULT			(95 * 1000)	 /* Default is 95 degrees Celsius */
#define ALERT_CH_NUM				3

struct uniphier_thermal_dev {
	struct device		*dev;
	void __iomem		*base;

	struct thermal_zone_device *tz_dev;
	u32                     alert_en[ALERT_CH_NUM];
};

struct uniphier_thermal_zone {
	void __iomem		*base;
};

static inline void maskwritel(void __iomem *base, u32 offset, u32 mask, u32 v)
{
	u32 tmp = readl_relaxed(base + offset);
	tmp &= mask;
	tmp |= v & mask;
	writel_relaxed(v, base + offset);
}

static inline u32 maskreadl(void __iomem *base, u32 offset, u32 mask)
{
	return (readl(base + offset) & mask) * 1000;	/* millicelsius */
}

static int uniphier_thermal_initialize_sensor(struct uniphier_thermal_dev *tdev)
{
	void __iomem *base = tdev->base;

	/* stop PVT control */
	maskwritel(base, PMPVTCTLENA_OFFSET,
		    PMPVTCTLENA_PMPVTCTLEN_A, PMPVTCTLENA_PMPVTCTLEN_A_STOP);

	/* set up default if missing eFuse */
	if (readl(base + TMODCOEF_OFFSET) == 0x0) {
		maskwritel(base, SETFANPARAM1_OFFSET,
			    SETFANPARAM1_ETMODINTERCEPTSEL |
			      SETFANPARAM1_ETMODINTERCEPT |
			      SETFANPARAM1_ETMODSLOPESEL |
			      SETFANPARAM1_ETMODSLOPE,
			    SETFANPARAM1_ETMODINTERCEPTSEL_ETMODINTERCEPT |
			      SETFANPARAM1_ETMODINTERCEPT_SETTINGS |
			      SETFANPARAM1_ETMODSLOPESEL_ETMODSLOPE |
			      SETFANPARAM1_ETMODSLOPE_SETTINGS);
	}

	/* set mode of temperature monitor */
	maskwritel(base, PMPVTCTLMODEA_OFFSET,
		    PMPVTCTLMODEA_PMPVTCTLMODE_A,
		    PMPVTCTLMODEA_PMPVTCTLMODE_A_TEMPMON);

	/* set period (ENDLESS, 100ms) */
	maskwritel(base, EMONREPEATA_OFFSET,
		    EMONREPEATA_EMONENDLESS_A |
		      EMONREPEATA_EMONNUM_A |
		      EMONREPEATA_EMONPERIOD_A,
		    EMONREPEATA_EMONENDLESS_A_ENABLE |
		      EMONREPEATA_EMONPERIOD_A_1000000);

	/* set UNIT_A(Main) */
	maskwritel(base, PMPVTCTLMODESEL_OFFSET,
		    PMPVTCTLMODESEL_PMPVTCTLMODESEL,
		    PMPVTCTLMODESEL_PMPVTCTLMODESEL_MAIN);

	return 0;
}

static int uniphier_thermal_set_limit(struct uniphier_thermal_dev *tdev, u32 ch, u32 temp_limit)
{
	void __iomem *base = tdev->base;

	if (ch >= ALERT_CH_NUM)
		return -EINVAL;

	/* set alert(ch) temperature */
	maskwritel(base, SETALERT0_OFFSET + (ch << 2),
		    SETALERT_EALERTTEMP0 |
		      SETALERT_EOFUFSEL0 |
		      SETALERT_EALERTEN0,
		    SETALERT_EALERTEN0_USE |
		      (temp_limit << 8));

	return 0;
}

static int uniphier_thermal_enable_sensor(struct uniphier_thermal_dev *tdev)
{
	void __iomem *base = tdev->base;
	u32 bits = 0;
	int i;

	for(i = 0; i < ALERT_CH_NUM; i++)
		bits |= (tdev->alert_en[i] * PMALERTINTCTL_ALERTINT_EN(i));

	/* enable alert interrupt */
	maskwritel(base, PMALERTINTCTL_OFFSET, PMALERTINTCTL_ALL_BITS, bits);

	/* start PVT control */
	maskwritel(base, PMPVTCTLENA_OFFSET,
		    PMPVTCTLENA_PMPVTCTLEN_A, PMPVTCTLENA_PMPVTCTLEN_A_START);

	return 0;
}

static int uniphier_thermal_disable_sensor(struct uniphier_thermal_dev *tdev)
{
	void __iomem *base = tdev->base;

	/* disable alert interrupt */
	maskwritel(base, PMALERTINTCTL_OFFSET, PMALERTINTCTL_ALL_BITS, 0);

	/* stop PVT control */
	maskwritel(base, PMPVTCTLENA_OFFSET,
		    PMPVTCTLENA_PMPVTCTLEN_A, PMPVTCTLENA_PMPVTCTLEN_A_STOP);

	return 0;
}

static int __uniphier_thermal_get_temp(void __iomem *base)
{
	return maskreadl(base, TMOD_OFFSET, TMOD_V_TMOD);
}

static int uniphier_thermal_get_temp(void *data, int *out_temp)
{
	struct uniphier_thermal_zone *zone = data;

	*out_temp = __uniphier_thermal_get_temp(zone->base);

	return 0;
}

static const struct thermal_zone_of_device_ops uniphier_of_thermal_ops = {
	.get_temp = uniphier_thermal_get_temp,
};

static void __uniphier_thermal_irq_clear(void __iomem *base)
{
	u32 mask = 0, bits = 0;
	int i;

	for(i = 0; i < ALERT_CH_NUM; i++) {
		mask |= (PMALERTINTCTL_ALERTINT_CLR(i) |
			 PMALERTINTCTL_ALERTINT_ST(i));
		bits |= PMALERTINTCTL_ALERTINT_CLR(i);
	}

	/* clear alert interrupt */
	maskwritel(base, PMALERTINTCTL_OFFSET, mask, bits);

	return;
}

static irqreturn_t uniphier_thermal_alarm_handler(int irq, void *_tdev)
{
	struct uniphier_thermal_dev *tdev = _tdev;

	dev_dbg(tdev->dev, "thermal alarm\n");

	__uniphier_thermal_irq_clear(tdev->base);

	thermal_zone_device_update(tdev->tz_dev);

	return IRQ_HANDLED;
}

static int uniphier_thermal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct uniphier_thermal_dev *tdev;
	struct uniphier_thermal_zone *zone;
	const struct thermal_trip *trips;
	int i, ret, irq;
	u32 temp_limit = TEMP_LIMIT_DEFAULT;

	tdev = devm_kzalloc(dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, tdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tdev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(tdev->base))
		return PTR_ERR(tdev->base);

	tdev->dev = dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -EINVAL;
	}

	/* register sensor */
	zone = devm_kzalloc(dev, sizeof(*zone), GFP_KERNEL);
	if (!zone) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	zone->base = tdev->base;
	tdev->tz_dev = thermal_zone_of_sensor_register(dev, 0, zone,
					     &uniphier_of_thermal_ops);
	if (IS_ERR(tdev->tz_dev)) {
		dev_err(dev, "failed to register thermal zone\n");
		return PTR_ERR(tdev->tz_dev);
	}

	/* get trips */
	trips = of_thermal_get_trip_points(tdev->tz_dev);
	if (of_thermal_get_ntrips(tdev->tz_dev) > ALERT_CH_NUM) {
		dev_err(dev, "ther number of trips exceeds %d\n", ALERT_CH_NUM);
		ret = -EINVAL;
		goto err;
	}
	for (i = 0; i < of_thermal_get_ntrips(tdev->tz_dev); i++) {
		if (trips[i].type == THERMAL_TRIP_CRITICAL) {
			temp_limit = trips[i].temperature;
			break;
		}
	}
	if (i == of_thermal_get_ntrips(tdev->tz_dev)) {
		dev_err(dev, "failed to find critical trip\n");
		ret = -EINVAL;
		goto err;
	}

	/* initialize sensor */
	uniphier_thermal_initialize_sensor(tdev);
	for (i = 0; i < ALERT_CH_NUM; i++) {
		tdev->alert_en[i] = 0;
	}
	for (i = 0; i < of_thermal_get_ntrips(tdev->tz_dev); i++) {
		uniphier_thermal_set_limit(tdev, i, trips[i].temperature);
		tdev->alert_en[i] = 1;
	}

	ret = devm_request_irq(dev, irq, uniphier_thermal_alarm_handler,
			       0, "uniphier_thermal", tdev);
	if (ret) {
		dev_err(dev, "failed to execute request_irq(%d)\n", res->start);
		goto err;
	}

	/* enable sensor */
	uniphier_thermal_enable_sensor(tdev);

	dev_info(dev, "thermal driver (temp_limit=%d C)\n",
		 temp_limit / 1000);

	return 0;

err:
	thermal_zone_of_sensor_unregister(dev, tdev->tz_dev);

	return ret;
}

static int uniphier_thermal_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct uniphier_thermal_dev *tdev = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(dev, tdev->tz_dev);

	/* disable sensor */
	uniphier_thermal_disable_sensor(tdev);

	return 0;
}

static const struct of_device_id uniphier_thermal_dt_ids[] = {
	{ .compatible = "socionext,uniphier-thermal" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, uniphier_thermal_of_match);

static struct platform_driver uniphier_thermal_driver = {
	.probe = uniphier_thermal_probe,
	.remove = uniphier_thermal_remove,
	.driver = {
		.name = "uniphier-thermal",
		.of_match_table = uniphier_thermal_dt_ids,
	},
};
module_platform_driver(uniphier_thermal_driver);

MODULE_AUTHOR("Kunihiko Hayashi <hayashi.kunihiko@socionext.com>");
MODULE_DESCRIPTION("UniPhier thermal management driver");
MODULE_LICENSE("GPL v2");
