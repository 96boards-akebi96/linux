// SPDX-License-Identifier: GPL-2.0

#include <linux/clk-provider.h>

#include "clk-uniphier.h"

static struct uniphier_clk_init_data ph1_ld11_clk_idata[] __initdata = {
	UNIPHIER_CLK_FACTOR("cpll", -1, "ref", 392, 5),		/* 1960 MHz */
	UNIPHIER_CLK_FACTOR("mpll", -1, "ref", 64, 1),		/* 1600 MHz */
	UNIPHIER_CLK_FACTOR("spll", -1, "ref", 80, 1),		/* 2000 MHz */
	UNIPHIER_CLK_FACTOR("vspll", -1, "ref", 80, 1),		/* 2000 MHz */
	UNIPHIER_CLK_FACTOR("uart", 3, "spll", 1, 34),
	UNIPHIER_CLK_FACTOR("fi2c", 4, "spll", 1, 40),
	UNIPHIER_CLK_GATE("ether-clken", -1, NULL, 0x2104, 6),
	UNIPHIER_CLK_GATE("ether", 8, "ether-clken", 0x200c, 6),
	UNIPHIER_CLK_GATE("stdmac-clken", -1, NULL, 0x210c, 8),
	UNIPHIER_CLK_GATE("stdmac", 10, "stdmac-clken", 0x200c, 8),
	/* CPU gears */
	UNIPHIER_CLK_DIV4("cpll", 2, 3, 4, 8),
	UNIPHIER_CLK_DIV4("mpll", 2, 3, 4, 8),
	UNIPHIER_CLK_DIV3("spll", 3, 4, 8),
	/* Note: both gear1 and gear4 are spll/4.  This is not a bug. */
	UNIPHIER_CLK_CPUGEAR("cpu-ca53", 33, 0x8080, 0xf, 8,
			     "cpll/2", "spll/4", "cpll/3", "spll/3",
			     "spll/4", "spll/8", "cpll/4", "cpll/8"),
	UNIPHIER_CLK_CPUGEAR("cpu-ipp", 34, 0x8100, 0xf, 8,
			     "mpll/2", "spll/4", "mpll/3", "spll/3",
			     "spll/4", "spll/8", "mpll/4", "mpll/8"),
	{ /* sentinel */ }
};

static void __init ph1_ld11_clk_init(struct device_node *np)
{
	uniphier_clk_init(np, ph1_ld11_clk_idata);
}
CLK_OF_DECLARE(ph1_ld11_clk, "socionext,ph1-ld11-sysctrl", ph1_ld11_clk_init);
