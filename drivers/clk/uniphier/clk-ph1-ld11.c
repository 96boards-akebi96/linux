// SPDX-License-Identifier: GPL-2.0

#include <linux/clk-provider.h>

#include "clk-uniphier.h"

static struct uniphier_clk_init_data ph1_ld11_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 80, 1),
	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 34),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 40),
	UNIPHIER_CLK_GATE("ether-clken", -1, NULL, 0x2104, 6),
	UNIPHIER_CLK_GATE("ether", 8, "ether-clken", 0x200c, 6),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x210c, 8),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x200c, 8),
	{ /* sentinel */ }
};

static void __init ph1_ld11_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_ld11_clk_idata);
}
CLK_OF_DECLARE(ph1_ld11_clk, "socionext,ph1-ld11-sysctrl", ph1_ld11_clk_init);
