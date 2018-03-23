/*
 * Copyright (C) 2015-2018 Socionext Inc.
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

static struct uniphier_clk_init_data ph1_pxs3_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 80, 1),
/*	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 34),*/
/*	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 40),*/
	UNIPHIER_CLK_GATE("ether0-clken", -1, NULL, 0x210c, 9),
	UNIPHIER_CLK_GATE("ether0", 8, "ether0-clken", 0x200c, 9),
	UNIPHIER_CLK_GATE("ether1-clken", -1, NULL, 0x210c, 10),
	UNIPHIER_CLK_GATE("ether1", 9, "ether1-clken", 0x200c, 10),
	UNIPHIER_CLK_GATE("usb30-clken",  -1, NULL, 0x210c, 4),
	UNIPHIER_CLK_GATE("usb30", -1, "usb30-clken", 0x200c, 4),
	UNIPHIER_CLK_GATE("usb30-hsphy-clken",  -1, "usb30", 0x210c, 16),
	UNIPHIER_CLK_GATE("usb30-ssphy0-clken", -1, "usb30-hsphy-clken", 0x210c, 18),
	UNIPHIER_CLK_GATE("usb30-ssphy1-clken", -1, "usb30-hsphy-clken", 0x210c, 20),
	UNIPHIER_CLK_GATE("usb30-phy0", 20, "usb30-ssphy0-clken", 0x200c, 16),
	UNIPHIER_CLK_GATE("usb30-phy1", 22, "usb30-ssphy0-clken", 0x200c, 18),
	UNIPHIER_CLK_GATE("usb30-phy2", 23, "usb30-ssphy1-clken", 0x200c, 20),
	UNIPHIER_CLK_GATE("usb31-clken0", -1, NULL, 0x210c, 5),
	UNIPHIER_CLK_GATE("usb31-clken1", -1, "usb31-clken0", 0x210c, 6),
	UNIPHIER_CLK_GATE("usb31", -1, "usb31-clken1", 0x200c, 5),
	UNIPHIER_CLK_GATE("usb31-hsphy-clken",  -1, "usb31", 0x210c, 17),
	UNIPHIER_CLK_GATE("usb31-ssphy0-clken", -1, "usb31-hsphy-clken", 0x210c, 19),
	UNIPHIER_CLK_GATE("usb31-phy0", 21, "usb31-ssphy0-clken", 0x200c, 17),
	UNIPHIER_CLK_GATE("usb31-phy1", 24, "usb31-ssphy0-clken", 0x200c, 19),
	UNIPHIER_CLK_GATEX("ahci-phy", -1, NULL, 0x200c, 21),
	UNIPHIER_CLK_GATE("ahci0-link-clken", -1, "ahci-phy", 0x210c, 7),
	UNIPHIER_CLK_GATE("ahci0-link", 25, "ahci0-link-clken", 0x200c, 7),
	UNIPHIER_CLK_GATE("ahci1-link-clken", 26, "ahci-phy", 0x210c, 8),
	UNIPHIER_CLK_GATE("ahci1-link", -1, "ahci1-link-clken", 0x200c, 8),
	UNIPHIER_CLK_GATE("pcie-clken", -1, NULL, 0x210c, 3),
	UNIPHIER_CLK_GATE("pcie", 27, "pcie-clken", 0x200c, 3),
	{ /* sentinel */ }
};

static void __init ph1_pxs3_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_pxs3_clk_idata);
}
CLK_OF_DECLARE(ph1_pxs3_clk, "socionext,uniphier-pxs3-sysctrl",
	       ph1_pxs3_clk_init);
