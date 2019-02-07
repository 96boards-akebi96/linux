/*
 * Copyright (C) 2016 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpufreq.h>
#include <linux/cpufreq-dt.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

/*
 * Machines for which the cpufreq device is *always* created, mostly used for
 * platforms using "operating-points" (V1) property.
 */
static const struct of_device_id whitelist[] __initconst = {
	{ }
};

/*
 * Machines for which the cpufreq device is *not* created, mostly used for
 * platforms using "operating-points-v2" property.
 */
static const struct of_device_id blacklist[] __initconst = {
	{ }
};

static bool __init cpu0_node_has_opp_v2_prop(void)
{
	struct device_node *np = of_cpu_device_node_get(0);
	bool ret = false;

	if (of_get_property(np, "operating-points-v2", NULL))
		ret = true;

	of_node_put(np);
	return ret;
}

static int __init cpufreq_dt_platdev_init(void)
{
	struct device_node *np = of_find_node_by_path("/");
	const struct of_device_id *match;
	const void *data = NULL;

	if (!np)
		return -ENODEV;

	match = of_match_node(whitelist, np);
	if (match) {
		data = match->data;
		goto create_pdev;
	}

	if (cpu0_node_has_opp_v2_prop() && !of_match_node(blacklist, np))
		goto create_pdev;

	of_node_put(np);
	return -ENODEV;

create_pdev:
	of_node_put(np);
	return PTR_ERR_OR_ZERO(platform_device_register_data(NULL, "cpufreq-dt",
			       -1, data,
			       sizeof(struct cpufreq_dt_platform_data)));
}
device_initcall(cpufreq_dt_platdev_init);
