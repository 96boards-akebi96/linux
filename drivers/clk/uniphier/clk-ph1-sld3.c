/*
 * Copyright (C) 2015 Masahiro Yamada <yamada.masahiro@socionext.com>
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

#include <linux/clk-provider.h>

#include "clk-uniphier.h"

static struct uniphier_clk_init_data ph1_sld3_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 65, 1),
	UNIPHIER_CLK_FACTOR("upll", -1, "ref", 288000, 24576),
	UNIPHIER_CLK_FACTOR("a2pll", -1, "ref", 24, 1),
	UNIPHIER_CLK_FACTOR("uart", 3, "a2pll", 1, 16),
	UNIPHIER_CLK_FACTOR("i2c", 4, "spll", 1, 16),
	UNIPHIER_CLK_FACTOR("arm-scu", 7, "spll", 1, 32),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x2104, 10),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x2000, 10),
	UNIPHIER_CLK_FACTOR("ehci", 18, "upll", 1, 12),
	{ /* sentinel */ }
};

static void __init ph1_sld3_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_sld3_clk_idata);
}
CLK_OF_DECLARE(ph1_sld3_clk, "socionext,ph1-sld3-sysctrl", ph1_sld3_clk_init);
