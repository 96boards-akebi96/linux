/*
 * Copyright (C) 2016 Masahiro Yamada <yamada.masahiro@socionext.com>
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

static struct uniphier_clk_init_data ph1_ld20_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 80, 1),
	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 34),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 40),
	UNIPHIER_CLK_GATE("ether-clken", -1, NULL, 0x2104, 6),
	UNIPHIER_CLK_GATE("ether", 8, "ether-clken", 0x200c, 6),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x210c, 8),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x200c, 8),
	UNIPHIER_CLK_GATE("usb30-clken",  -1, NULL, 0x210c, 14),
	UNIPHIER_CLK_GATE("usb30",  -1, NULL, 0x200c, 5),
	UNIPHIER_CLK_GATE("usb30-hsphy0-clken",  -1, "usb30", 0x210c, 12),
	UNIPHIER_CLK_GATE("usb30-hsphy1-clken",  -1, "usb30", 0x210c, 13),
	UNIPHIER_CLK_GATE("usb30-phy0", 20, "usb30-hsphy0-clken", 0x200c, 12),
	UNIPHIER_CLK_GATE("usb30-phy1", 21, "usb30-hsphy1-clken", 0x200c, 13),
	UNIPHIER_CLK_GATE("usb30-phy2", 22, "usb30-hsphy0-clken", 0x200c, 14),
	UNIPHIER_CLK_GATE("usb30-phy3", 23, "usb30-hsphy1-clken", 0x200c, 15),
	UNIPHIER_CLK_GATE("pcie-clken", -1, NULL, 0x210c, 4),
	UNIPHIER_CLK_GATE("pcie", 27, "pcie-clken", 0x200c, 4),
	{ /* sentinel */ }
};

static void __init ph1_ld20_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_ld20_clk_idata);
}
CLK_OF_DECLARE(ph1_ld20_clk, "socionext,ph1-ld20-sysctrl", ph1_ld20_clk_init);
