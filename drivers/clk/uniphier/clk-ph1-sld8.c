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

static struct uniphier_clk_init_data ph1_sld8_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 64, 1),		/* 1600 MHz */
	UNIPHIER_CLK_FACTOR("upll", -1, "ref", 288, 25),	/* 288 MHz */
	UNIPHIER_CLK_FACTOR("vpll27a", -1, "ref", 270, 25),	/* 270 MHz */
	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 20),
	UNIPHIER_CLK_FACTOR("i2c", 4, "spll", 1, 16),
	UNIPHIER_CLK_FACTOR("spi", 5, "spll", 1, 32),
	UNIPHIER_CLK_FACTOR("arm-scu", 7, "spll", 1, 32),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x2104, 10),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x2000, 10),
	UNIPHIER_CLK_FACTOR("ehci", 18, "upll", 1, 12),
	{ /* sentinel */ }
};

static void __init ph1_sld8_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_sld8_clk_idata);
}
CLK_OF_DECLARE(ph1_sld8_clk, "socionext,ph1-sld8-sysctrl", ph1_sld8_clk_init);
