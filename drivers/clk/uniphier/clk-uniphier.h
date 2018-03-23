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

enum uniphier_clk_type {
	UNIPHIER_CLK_TYPE_FIXED_FACTOR,
	UNIPHIER_CLK_TYPE_FIXED_RATE,
	UNIPHIER_CLK_TYPE_GATE,
	UNIPHIER_CLK_TYPE_MUX,
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
		struct uniphier_clk_fixed_factor_data factor;
		struct uniphier_clk_fixed_rate_data rate;
		struct uniphier_clk_gate_data gate;
		struct uniphier_clk_mux_data mux;
	} data;
	struct clk *clk;
};

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

int uniphier_clk_init(struct device_node *np,
		      struct uniphier_clk_init_data *idata);

#endif /* __CLK_UNIPHIER_H__ */
