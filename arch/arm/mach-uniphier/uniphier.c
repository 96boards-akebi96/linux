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

#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/mach/arch.h>

static const char * const uniphier_dt_compat[] __initconst = {
	"socionext,ph1-sld3",
	"socionext,ph1-ld4",
	"socionext,ph1-pro4",
	"socionext,ph1-sld8",
	"socionext,ph1-pro5",
	"socionext,proxstream2",
	"socionext,ph1-ld6b",
	NULL,
};

static const struct of_device_id uniphier_aidet_of_device_ids[] __initconst = {
	{ .compatible = "socionext,uniphier-aidet" },
	{ /* sentinel */ }
};

static void __init uniphier_init_irq(void)
{
	struct device_node *np;
	void __iomem *reg;
	u32 tmp;

	reg = ioremap(0x55000090, 4); /* IRQCTL */
	if (!reg)
		goto err;
	tmp = readl(reg);
	tmp |= 0x000000ff;
	iowrite32(tmp, reg);
	iounmap(reg);

	np = of_find_matching_node_and_match(NULL,
					     uniphier_aidet_of_device_ids,
					     NULL);
	reg = of_iomap(np, 0);
	if (!reg)
		goto err;
	tmp = readl(reg + 0x8);
	tmp |= 0xff << 16;
	writel(tmp, reg + 0x8);
	iounmap(reg);

err:
	irqchip_init();
}

DT_MACHINE_START(UNIPHIER, "Socionext UniPhier")
	.dt_compat	= uniphier_dt_compat,
	.init_irq	= uniphier_init_irq,
MACHINE_END
