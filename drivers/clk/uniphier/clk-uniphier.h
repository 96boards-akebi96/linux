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

#ifndef __CLK_UNIPHIER_H__
#define __CLK_UNIPHIER_H__

#include <linux/kernel.h>

#define UNIPHIER_CLK_CPUGEAR_MAX_PARENTS	16

enum uniphier_clk_type {
	UNIPHIER_CLK_TYPE_CPUGEAR,
	UNIPHIER_CLK_TYPE_FIXED_FACTOR,
	UNIPHIER_CLK_TYPE_FIXED_RATE,
	UNIPHIER_CLK_TYPE_GATE,
	UNIPHIER_CLK_TYPE_MUX,
};

struct uniphier_clk_cpugear_data {
	const char *parent_names[UNIPHIER_CLK_CPUGEAR_MAX_PARENTS];
	unsigned int num_parents;
	unsigned int regbase;
	unsigned int mask;
};

struct uniphier_clk_fixed_factor_data {
	const char *parent_name;
	unsigned int mult;
	unsigned int div;
};

struct uniphier_clk_fixed_rate_data {
	unsigned long fixed_rate;
};

struct uniphier_clk_gate_data {
	const char *parent_name;
	unsigned int reg;
	u8 bit_idx;
	u8 flags;
};

struct uniphier_clk_mux_data {
	const char *parent_names[4];
	u8 num_parents;
	unsigned int reg;
	u8 shift;
	u8 flags;
};

struct uniphier_clk_init_data {
	const char *name;
	enum uniphier_clk_type type;
	int output_index;
	union {
		struct uniphier_clk_cpugear_data cpugear;
		struct uniphier_clk_fixed_factor_data factor;
		struct uniphier_clk_fixed_rate_data rate;
		struct uniphier_clk_gate_data gate;
		struct uniphier_clk_mux_data mux;
	} data;
	struct clk *clk;
};

#define UNIPHIER_CLK_CPUGEAR(_name, _idx, _regbase, _mask,	\
			     _num_parents, ...)			\
	{							\
		.name = (_name),				\
		.type = UNIPHIER_CLK_TYPE_CPUGEAR,		\
		.output_index = (_idx),				\
		.data.cpugear = {				\
			.parent_names = { __VA_ARGS__ },	\
			.num_parents = (_num_parents),		\
			.regbase = (_regbase),			\
			.mask = (_mask)				\
		 },						\
	}

#define UNIPHIER_CLK_FACTOR(_name, _idx, _parent, _mult, _div)	\
	{							\
		.name = (_name),				\
		.type = UNIPHIER_CLK_TYPE_FIXED_FACTOR,		\
		.output_index = (_idx),				\
		.data.factor = {				\
			.parent_name = (_parent),		\
			.mult = (_mult),			\
			.div = (_div),				\
		},						\
	}

#define UNIPHIER_CLK_GATE(_name, _idx, _parent, _reg, _bit)	\
	{							\
		.name = (_name),				\
		.type = UNIPHIER_CLK_TYPE_GATE,			\
		.output_index = (_idx),				\
		.data.gate = {					\
			.parent_name = (_parent),		\
			.reg = (_reg),				\
			.bit_idx = (_bit),			\
		},						\
	}

#define UNIPHIER_CLK_GATEX(_name, _idx, _parent, _reg, _bit)	\
	{							\
		.name = (_name),				\
		.type = UNIPHIER_CLK_TYPE_GATE,			\
		.output_index = (_idx),				\
		.data.gate = {					\
			.parent_name = (_parent),		\
			.reg = (_reg),				\
			.bit_idx = (_bit),			\
			.flags = CLK_GATE_SET_TO_DISABLE,	\
		},						\
	}

#define UNIPHIER_CLK_DIV(parent, div)				\
	UNIPHIER_CLK_FACTOR(parent "/" #div, -1, parent, 1, div)

#define UNIPHIER_CLK_DIV2(parent, div0, div1)			\
	UNIPHIER_CLK_DIV(parent, div0),				\
	UNIPHIER_CLK_DIV(parent, div1)

#define UNIPHIER_CLK_DIV3(parent, div0, div1, div2)		\
	UNIPHIER_CLK_DIV2(parent, div0, div1),			\
	UNIPHIER_CLK_DIV(parent, div2)

#define UNIPHIER_CLK_DIV4(parent, div0, div1, div2, div3)	\
	UNIPHIER_CLK_DIV2(parent, div0, div1),			\
	UNIPHIER_CLK_DIV2(parent, div2, div3)

struct clk *uniphier_clk_register_cpugear(struct device *dev, const char *name,
					  const char * const *parent_names,
					  u8 num_parents,
					  void __iomem *regbase, u32 mask);

int uniphier_clk_init(struct device_node *np,
		      struct uniphier_clk_init_data *idata);

#endif /* __CLK_UNIPHIER_H__ */
