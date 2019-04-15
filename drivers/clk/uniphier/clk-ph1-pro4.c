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

static struct uniphier_clk_init_data ph1_pro4_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 64, 1),		/* 1600 MHz */
	UNIPHIER_CLK_FACTOR("upll", -1, "ref", 288, 25),	/* 288 MHz */
	UNIPHIER_CLK_FACTOR("a2pll", -1, "upll", 256, 125),	/* 589.824 MHz */
	UNIPHIER_CLK_FACTOR("vpll27a", -1, "ref", 270, 25),	/* 270 MHz */
	UNIPHIER_CLK_FACTOR("gpll", -1, "ref", 10, 1),		/* 250 MHz */
	UNIPHIER_CLK_FACTOR("uart", 3, "a2pll", 1, 8),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 32),
	UNIPHIER_CLK_FACTOR("spi", 5, "spll", 1, 32),
	UNIPHIER_CLK_FACTOR("arm-scu", 7, "spll", 1, 32),
	UNIPHIER_CLK_GATE("gio-clken", -1, NULL, 0x2104, 6),
	UNIPHIER_CLK_GATE("gio", -1, "gio-clken", 0x2000, 6),
	UNIPHIER_CLK_GATE("ether-clken", -1, "gio", 0x2104, 12),
	UNIPHIER_CLK_GATE("ether-gb-clken", -1, "ether-clken", 0x2104, 5),
	UNIPHIER_CLK_GATE("ether-phy-clken", -1, "ether-gb-clken", 0x2260, 0),
	UNIPHIER_CLK_GATE("ether", 8, "ether-phy-clken", 0x2000, 12),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, "ref", 0x2104, 10),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x2000, 10),
	UNIPHIER_CLK_FACTOR("ehci", 18, "upll", 1, 12),
	UNIPHIER_CLK_GATE("usb30-clken", -1, "gio", 0x2104, 16),
	UNIPHIER_CLK_GATE("usb30", 20, "usb30-clken", 0x2000, 17),
	UNIPHIER_CLK_GATE("usb31-clken", -1, "gio", 0x2104, 17),
	UNIPHIER_CLK_GATE("usb31", 21, "usb31-clken", 0x2004, 17),
	UNIPHIER_CLK_GATE("ahci0-phy", -1, "gio", 0x2000, 19),
	UNIPHIER_CLK_GATE("ahci0-clken", -1, "ahci-phy", 0x2000, 18),
	UNIPHIER_CLK_GATE("ahci0", 25, "ahci0-clken", 0x2014, 18),
	UNIPHIER_CLK_GATE("ahci1-clken", -1, "ahci-phy", 0x2004, 18),
	UNIPHIER_CLK_GATE("ahci1", 26, "ahci1-clken", 0x2014, 19),
	{ /* sentinel */ }
};

static void __init ph1_pro4_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_pro4_clk_idata);
}
CLK_OF_DECLARE(ph1_pro4_clk, "socionext,ph1-pro4-sysctrl", ph1_pro4_clk_init);
