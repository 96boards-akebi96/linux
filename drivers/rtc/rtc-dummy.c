/* drivers/rtc/rtc-dummy.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (C) 2017 Imagination Technologies Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

struct dummy_rtc {
	struct rtc_device *rtc;
    uint32_t enabled;
    uint32_t pending;
    uint32_t irq_enabled;
};

static DEFINE_SPINLOCK(dummy_rtc_lock);

static int dummy_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	// current time + 1s
	rtc_time64_to_tm(ktime_get_real_seconds() + 1, tm);

	return 0;
}

static int dummy_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct timespec64 time = {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	int err = 0;

	time.tv_sec = rtc_tm_to_time64(tm);
	err = do_settimeofday64(&time);
	return 0;
}

static int dummy_rtc_read_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
    struct dummy_rtc *priv = dev_get_drvdata(dev);

	if (priv == NULL) {
        dev_err(dev, "%s:%d priv is NULL", __func__, __LINE__);
        return -1;
    }

    // dev_info(dev, "%s:%d read alrm  : \n", __func__, __LINE__);
    alrm->enabled = priv->enabled;
    alrm->pending = priv->pending;
	return 0;
}

struct timer_list rtc_timer;

static void rtc_timer_func(unsigned long arg)
{ 
    unsigned long flags = 0;
    struct device *dev = (struct device *)arg;
    struct dummy_rtc *priv = dev_get_drvdata(dev);

    spin_lock_irqsave(&dummy_rtc_lock, flags);

    priv->pending = 1;
    if (priv->irq_enabled) {
        rtc_update_irq(priv->rtc, 1, RTC_IRQF | RTC_AF);
    }
    spin_unlock_irqrestore(&dummy_rtc_lock, flags);
}

static int dummy_rtc_set_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
    struct dummy_rtc *priv = dev_get_drvdata(dev);
    unsigned long flags = 0;
    int ret = 0;
    struct timespec64 timer;

	if (priv == NULL) {
        dev_err(dev, "%s:%d priv is NULL", __func__, __LINE__);
        return -1;
    }

    spin_lock_irqsave(&dummy_rtc_lock, flags);
    // dev_info(dev, "%s:%d alrm enabled : %d %d\n", __func__, __LINE__, alrm->enabled, alrm->time);

    if (priv->enabled == 1) {
        del_timer(&rtc_timer);
    }
	if (alrm->enabled) {
        priv->enabled = 1;
        priv->pending = 0;

        init_timer(&rtc_timer);
        timer.tv_sec = rtc_tm_to_time64(&alrm->time) - ktime_get_real_seconds();
        timer.tv_nsec = 0;
        rtc_timer.expires = jiffies + timespec_to_jiffies(&timer);
        rtc_timer.data = (unsigned long)dev;
        rtc_timer.function = rtc_timer_func;
        add_timer(&rtc_timer);
	} else {
        priv->enabled = 0;
	}
    spin_unlock_irqrestore(&dummy_rtc_lock, flags);
	return ret;
}

static int dummy_rtc_alarm_irq_enable(struct device *dev, unsigned int enable)
{
    struct dummy_rtc *priv = dev_get_drvdata(dev);
    unsigned long flags = 0;

	if (priv == NULL) {
        dev_err(dev, "%s:%d priv is NULL", __func__, __LINE__);
        return -1;
    }

    spin_lock_irqsave(&dummy_rtc_lock, flags);

    // dev_info(dev, "%s:%d alrm irq enabled : %d\n", __func__, __LINE__, enable);
    priv->irq_enabled = enable;

    spin_unlock_irqrestore(&dummy_rtc_lock, flags);

	return 0;
}

static const struct rtc_class_ops dummy_rtc_ops = {
	.read_time	= dummy_rtc_read_time,
	.set_time	= dummy_rtc_set_time,
	.read_alarm	= dummy_rtc_read_alarm,
	.set_alarm	= dummy_rtc_set_alarm,
	.alarm_irq_enable = dummy_rtc_alarm_irq_enable
};

static int dummy_rtc_probe(struct platform_device *pdev)
{
	struct dummy_rtc *rtcdrv;

	rtcdrv = devm_kzalloc(&pdev->dev, sizeof(*rtcdrv), GFP_KERNEL);
	if (!rtcdrv)
		return -ENOMEM;

	platform_set_drvdata(pdev, rtcdrv);

	rtcdrv->rtc = devm_rtc_device_register(&pdev->dev, pdev->name,
					       &dummy_rtc_ops,
					       THIS_MODULE);
	if (IS_ERR(rtcdrv->rtc))
		return PTR_ERR(rtcdrv->rtc);

	return 0;
}

static const struct of_device_id dummy_rtc_of_match[] = {
	{ .compatible = "socionext,dummy_rtc", },
	{},
};
MODULE_DEVICE_TABLE(of, dummy_rtc_of_match);

static struct platform_driver dummy_rtc = {
	.probe = dummy_rtc_probe,
	.driver = {
		.name = "dummy_rtc",
		.of_match_table = dummy_rtc_of_match,
	}
};

module_platform_driver(dummy_rtc);

MODULE_AUTHOR("foo@socionext.com");
MODULE_DESCRIPTION("dummy RTC driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:rtc-dummy");
