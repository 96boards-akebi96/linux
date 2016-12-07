/*
 * Copyright (C) 2016 Socionext Inc.
 *   Author: Masahiro Yamada <yamada.masahiro@socionext.com>
 *   Ported: Kunihiko Hayashi <hayashi.kunihiko@socionext.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/slab.h>

#include "clk-uniphier.h"

#define UNIPHIER_CLK_CPUGEAR_STAT	0	/* status */
#define UNIPHIER_CLK_CPUGEAR_SET	4	/* set */
#define UNIPHIER_CLK_CPUGEAR_UPD	8	/* update */
#define   UNIPHIER_CLK_CPUGEAR_UPD_BIT	BIT(0)

struct uniphier_clk_cpugear {
	struct clk_hw hw;
	void __iomem *regbase;
	unsigned int mask;
	unsigned int flags;
};

#define to_uniphier_clk_cpugear(_hw) \
			container_of(_hw, struct uniphier_clk_cpugear, hw)

static int uniphier_clk_cpugear_set_parent(struct clk_hw *hw, u8 index)
{
	struct uniphier_clk_cpugear *gear = to_uniphier_clk_cpugear(hw);
	u32 val;

	val = readl(gear->regbase + UNIPHIER_CLK_CPUGEAR_SET);
	val &= ~gear->mask;
	val |= index;
	writel(val, gear->regbase + UNIPHIER_CLK_CPUGEAR_SET);

	val = readl(gear->regbase + UNIPHIER_CLK_CPUGEAR_UPD);
	val |= UNIPHIER_CLK_CPUGEAR_UPD_BIT;
	writel(val, gear->regbase + UNIPHIER_CLK_CPUGEAR_UPD);

	return readl_poll_timeout(gear->regbase + UNIPHIER_CLK_CPUGEAR_UPD,
				  val, !(val & UNIPHIER_CLK_CPUGEAR_UPD_BIT),
				  0, 1);
}

static u8 uniphier_clk_cpugear_get_parent(struct clk_hw *hw)
{
	struct uniphier_clk_cpugear *gear = to_uniphier_clk_cpugear(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	val = readl(gear->regbase + UNIPHIER_CLK_CPUGEAR_STAT);
	val &= gear->mask;

	return val < num_parents ? val : -EINVAL;
}

static const struct clk_ops uniphier_clk_cpugear_ops = {
	.determine_rate = __clk_mux_determine_rate,
	.set_parent = uniphier_clk_cpugear_set_parent,
	.get_parent = uniphier_clk_cpugear_get_parent,
};

struct clk *uniphier_clk_register_cpugear(struct device *dev, const char *name,
					  const char * const *parent_names,
					  u8 num_parents,
					  void __iomem *regbase, u32 mask)
{
	struct uniphier_clk_cpugear *gear;
	struct clk *clk;
	struct clk_init_data init;

	gear = kzalloc(sizeof(*gear), GFP_KERNEL);
	if (!gear)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &uniphier_clk_cpugear_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	gear->regbase = regbase;
	gear->mask = mask;
	gear->hw.init = &init;

	clk = clk_register(dev, &gear->hw);
	if (IS_ERR(clk))
		kfree(gear);

	return clk;
}

void uniphier_clk_unregister_cpugear(struct clk *clk)
{
	struct uniphier_clk_cpugear *gear;
	struct clk_hw *hw;

	hw = __clk_get_hw(clk);
	if (!hw)
		return;

	gear = to_uniphier_clk_cpugear(hw);

	clk_unregister(clk);
	kfree(gear);
}
