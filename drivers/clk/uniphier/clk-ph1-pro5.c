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

static struct uniphier_clk_init_data ph1_pro5_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 120, 1),		/* 2400 MHz */
	UNIPHIER_CLK_FACTOR("dapll1", -1, "ref", 128, 125),	/* 2560 MHz */
	UNIPHIER_CLK_FACTOR("dapll2", -1, "dapll1", 144, 125),	/* 2949.12 MHz */
	UNIPHIER_CLK_FACTOR("uart", 3, "dapll2", 1, 8),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 48),
	UNIPHIER_CLK_FACTOR("arm-scu", 7, "spll", 1, 48),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, "ref", 0x2104, 10),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x2000, 10),
	UNIPHIER_CLK_GATE("gio-clken", -1, NULL, 0x2104, 6),
	UNIPHIER_CLK_GATE("gio", -1, "gio-clken", 0x2000, 6),
	UNIPHIER_CLK_GATE("usb30-clken", 20, "gio", 0x2104, 16),
	UNIPHIER_CLK_GATE("usb30", -1, "usb30-clken", 0x2000, 17),
	UNIPHIER_CLK_GATE("usb31-clken", 21, "gio", 0x2104, 17),
	UNIPHIER_CLK_GATE("usb31", -1, "usb31-clken", 0x2004, 17),
	UNIPHIER_CLK_GATE("pcie-clken", -1, "gio", 0x2108, 2),
	UNIPHIER_CLK_GATE("pcie", 27, "pcie-clken", 0x2008, 2),
	{ /* sentinel */ }
};

static void __init ph1_pro5_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_pro5_clk_idata);
}
CLK_OF_DECLARE(ph1_pro5_clk, "socionext,ph1-pro5-sysctrl", ph1_pro5_clk_init);
