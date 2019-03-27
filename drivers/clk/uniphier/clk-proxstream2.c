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

static struct uniphier_clk_init_data proxstream2_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 96, 1),		/* 2400 MHz */
	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 27),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 48),
	UNIPHIER_CLK_FACTOR("spi", -1, "spll", 1, 48),
	UNIPHIER_CLK_FACTOR("arm-scu", 7, "spll", 1, 48),
	UNIPHIER_CLK_GATE("ether-clken", -1, NULL, 0x2104, 12),
	UNIPHIER_CLK_GATE("ether", 8, "ether-clken", 0x2000, 12),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x2104, 10),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x2000, 10),
	UNIPHIER_CLK_GATE("usb30-clken", -1, NULL, 0x2104, 16),
	UNIPHIER_CLK_GATE("usb30", -1, "usb30-clken", 0x2000, 17),
	UNIPHIER_CLK_GATE("usb30-hsphy-clken", -1, "usb30", 0x2104, 19),
	UNIPHIER_CLK_GATE("usb30-phy0", 20, "usb30-hsphy-clken", 0x2014, 4),
	UNIPHIER_CLK_GATE("usb30-phy1", 22, "usb30-hsphy-clken", 0x2014, 0),
	UNIPHIER_CLK_GATE("usb30-phy2", 23, "usb30-hsphy-clken", 0x2014, 2),
	UNIPHIER_CLK_GATE("usb31-clken", -1, NULL, 0x2104, 17),
	UNIPHIER_CLK_GATE("usb31", -1, "usb31-clken", 0x2004, 17),
	UNIPHIER_CLK_GATE("usb31-hsphy-clken", -1, "usb31", 0x2104, 20),
	UNIPHIER_CLK_GATE("usb31-phy0", 21, "usb31-hsphy-clken", 0x2014, 5),
	UNIPHIER_CLK_GATE("usb31-phy1", 24, "usb31-hsphy-clken", 0x2014, 1),
	UNIPHIER_CLK_GATEX("ahci-phy", -1, NULL, 0x2014, 8),
	UNIPHIER_CLK_GATE("ahci-clken", -1, "ahci-phy", 0x2104, 22),
	UNIPHIER_CLK_GATE("ahci", 25, "ahci-clken", 0x2014, 12),
	{ /* sentinel */ }
};

static void __init proxstream2_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, proxstream2_clk_idata);
}
CLK_OF_DECLARE(proxstream2_clk, "socionext,proxstream2-sysctrl",
	       proxstream2_clk_init);
