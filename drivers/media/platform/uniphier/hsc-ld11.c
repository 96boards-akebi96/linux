// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
// For UniPhier LD11/LD20.
//
// Copyright (c) 2018 Socionext Inc.

#include "hsc.h"
#include "hsc-reg.h"

static const struct hsc_spec_init_ram uniphier_hsc_ld11_init_rams[] = {
	{ FLT_PATN_RAM_TOP_ADDR, FLT_PATN_RAM_SIZE, ~0, },
	{ FLT_MASK_RAM_TOP_ADDR, FLT_MASK_RAM_SIZE, 0, },
	/* FLT_PID Pattern */
	{ SHARE_MEMORY_0_NORMAL, FLT_PIDPATTERN_SIZE, ~0, },
	/* FLT_PID Table */
	{ SHARE_MEMORY_0_NORMAL + FLT_PIDPATTERN_SIZE,
	  SHARE_MEMORY_0_SIZE - FLT_PIDPATTERN_SIZE, 0, },
	{ SHARE_MEMORY_1_NORMAL, SHARE_MEMORY_1_SIZE, 0, },
	{ SHARE_MEMORY_2_NORMAL, SHARE_MEMORY_2_SIZE, 0, },
	{ SHARE_MEMORY_3_NORMAL, SHARE_MEMORY_3_SIZE, 0, },
	{ SHARE_MEMORY_4_NORMAL, SHARE_MEMORY_4_SIZE, 0, },
	{ SHARE_MEMORY_5_NORMAL, SHARE_MEMORY_5_SIZE, 0, },
};

static const struct hsc_spec_init_ram uniphier_hsc_ld20_init_rams[] = {
	{ FLT_PATN_RAM_TOP_ADDR, FLT_PATN_RAM_SIZE, ~0, },
	{ FLT_MASK_RAM_TOP_ADDR, FLT_MASK_RAM_SIZE, 0, },
	/* FLT_PID Pattern */
	{ SHARE_MEMORY_0_NORMAL, FLT_PIDPATTERN_SIZE, ~0, },
	/* FLT_PID Table */
	{ SHARE_MEMORY_0_NORMAL + FLT_PIDPATTERN_SIZE,
	  SHARE_MEMORY_0_SIZE - FLT_PIDPATTERN_SIZE, 0, },
	{ SHARE_MEMORY_1_NORMAL, SHARE_MEMORY_1_SIZE, 0, },
	{ SHARE_MEMORY_2_NORMAL, SHARE_MEMORY_2_SIZE, 0, },
	{ SHARE_MEMORY_3_NORMAL, SHARE_MEMORY_3_SIZE, 0, },
	{ SHARE_MEMORY_4_NORMAL, SHARE_MEMORY_4_SIZE, 0, },
	{ SHARE_MEMORY_5_NORMAL, SHARE_MEMORY_5_SIZE, 0, },
	{ SHARE_MEMORY_6_NORMAL, SHARE_MEMORY_6_SIZE, 0, },
	{ SHARE_MEMORY_7_NORMAL, SHARE_MEMORY_7_SIZE, 0, },
};

static const struct hsc_spec_css uniphier_hsc_ld11_css_in[] = {
	[HSC_CSS_IN_SRLTS0] = {
		.pol = { true, CSS_SIGNALPOLCH(0), -1,  3,  0, },
	},
	[HSC_CSS_IN_SRLTS1] = {
		.pol = { true, CSS_SIGNALPOLCH(0), -1, 11,  8, },
	},
	[HSC_CSS_IN_SRLTS2] = {
		.pol = { true, CSS_SIGNALPOLCH(0), -1, 19, 16, },
	},
	[HSC_CSS_IN_SRLTS3] = {
		.pol = { true, CSS_SIGNALPOLCH(0), -1, 27, 24, },
	},
	[HSC_CSS_IN_SRLTS4] = {
		.pol = { true, CSS_SIGNALPOLCH(1), -1,  3,  0, },
	},
	[HSC_CSS_IN_PARTS0] = {
		.pol = { true, CSS_PTSISIGNALPOL,  -1, -1,  0, },
	},
	[HSC_CSS_IN_PARTS1] = {
		.pol = { true, CSS_PTSISIGNALPOL,  -1, -1,  8, },
	},
	[HSC_CSS_IN_DMD0]   = {
		.pol = { true, CSS_DMDSIGNALPOL,   -1, -1, 16, },
	},
};

static const struct hsc_spec_css uniphier_hsc_ld11_css_out[] = {
	[HSC_CSS_OUT_SRLTS0] = {
		.pol = { true, CSS_STSOSIGNALPOL,   6, -1,  0, },
		.sel = { true, CSS_OUTPUTCTRL(0), GENMASK(4,  0), 0, },
	},
	[HSC_CSS_OUT_SRLTS1] = {
		.pol = { true, CSS_STSOSIGNALPOL,  14, -1,  8, },
		.sel = { true, CSS_OUTPUTCTRL(0), GENMASK(12, 8), 8, },
	},
	[HSC_CSS_OUT_TSI0] = {
		.sel = { true, CSS_OUTPUTCTRL(1), GENMASK(4,  0), 0, },
	},
	[HSC_CSS_OUT_TSI1] = {
		.sel = { true, CSS_OUTPUTCTRL(1), GENMASK(12, 8), 8, },
	},
	[HSC_CSS_OUT_TSI2] = {
		.sel = { true, CSS_OUTPUTCTRL(1), GENMASK(20, 16), 16, },
	},
	[HSC_CSS_OUT_TSI3] = {
		.sel = { true, CSS_OUTPUTCTRL(1), GENMASK(28, 24), 24, },
	},
	[HSC_CSS_OUT_TSI4] = {
		.sel = { true, CSS_OUTPUTCTRL(2), GENMASK(4,  0), 0, },
	},
	[HSC_CSS_OUT_PARTS0] = {
		.pol = { true, CSS_PTSOSIGNALPOL,  -1, -1,  0, },
		.sel = { true, CSS_OUTPUTCTRL(4), GENMASK(4,  0), 0, },
	},
	[HSC_CSS_OUT_PKTFF0] = {
		.sel = { true, CSS_OUTPUTCTRL(5), GENMASK(4,  0), 0, },
	},
};

static const struct hsc_spec_ts uniphier_hsc_ld11_ts_in[] = {
	[HSC_TS_IN0] = {
		.intr = { true, IOB_INTREN0, 13 },
	},
	[HSC_TS_IN1] = {
		.intr = { true, IOB_INTREN0, 14 },
	},
	[HSC_TS_IN2] = {
		.intr = { true, IOB_INTREN0, 15 },
	},
	[HSC_TS_IN3] = {
		.intr = { true, IOB_INTREN0, 16 },
	},
	[HSC_TS_IN4] = {
		.intr = { true, IOB_INTREN0, 17 },
	},
};

static const struct hsc_spec_dma uniphier_hsc_ld11_dma_in[] = {
	[HSC_DMA_IN0] = {
		.dma_ch = 5,
		.rb_ch  = 4,
		.it_ch  = 0,
		.en     = { true, CDMBC_TDSTRT, 5 },
		.intr   = { true, IOB_INTREN1, 1 },
	},
	[HSC_DMA_IN1] = {
		.dma_ch = 6,
		.rb_ch  = 5,
		.it_ch  = 1,
		.en     = { true, CDMBC_TDSTRT, 6 },
		.intr   = { true, IOB_INTREN1, 2 },
	},
	[HSC_DMA_IN2] = {
		.dma_ch = 7,
		.rb_ch  = 6,
		.it_ch  = 2,
		.en     = { true, CDMBC_TDSTRT, 7 },
		.intr   = { true, IOB_INTREN1, 3 },
	},
	[HSC_DMA_IN3] = {
		.dma_ch = 22,
		.rb_ch  = 20,
		.it_ch  = 13,
		.en     = { true, CDMBC_TDSTRT, 13 },
		.intr   = { true, IOB_INTREN1, 4 },
	},
	[HSC_DMA_IN4] = {
		.dma_ch = 23,
		.rb_ch  = 21,
		.it_ch  = 14,
		.en     = { true, CDMBC_TDSTRT, 14 },
		.intr   = { true, IOB_INTREN1, 5 },
	},
	[HSC_DMA_IN5] = {
		.dma_ch = 24,
		.rb_ch  = 22,
		.it_ch  = 15,
		.en     = { true, CDMBC_TDSTRT, 15 },
		.intr   = { true, IOB_INTREN1, 6 },
	},
	[HSC_DMA_CIP_IN0] = {
		.dma_ch = 8,
		.rb_ch  = 7,
		.cip_ch = 0,
		.it_ch  = 3,
		.en     = { true, CDMBC_STRT(1), 0 },
		.intr   = { true, IOB_INTREN2, 4 },
	},
	[HSC_DMA_CIP_IN1] = {
		.dma_ch = 10,
		.rb_ch  = 9,
		.cip_ch = 1,
		.it_ch  = 5,
		.en     = { true, CDMBC_STRT(1), 2 },
		.intr   = { true, IOB_INTREN2, 5 },
	},
};

static const struct hsc_spec_dma uniphier_hsc_ld11_dma_out[] = {
	[HSC_DMA_OUT0] = {
		.dma_ch = 1,
		.rb_ch  = 1,
		.td_ch  = 0,
		.en     = { true, CDMBC_TDSTRT, 1 },
		.intr   = { true, IOB_INTREN1, 13 },
	},
	[HSC_DMA_OUT1] = {
		.dma_ch = 2,
		.rb_ch  = 2,
		.td_ch  = 2,
		.en     = { true, CDMBC_TDSTRT, 2 },
		.intr   = { true, IOB_INTREN1, 14 },
	},
	[HSC_DMA_OUT2] = {
		.dma_ch = 3,
		.rb_ch  = 3,
		.td_ch  = 4,
		.en     = { true, CDMBC_TDSTRT, 3 },
		.intr   = { true, IOB_INTREN1, 15 },
	},
	[HSC_DMA_OUT3] = {
		.dma_ch = 19,
		.rb_ch  = 17,
		.td_ch  = 1,
		.en     = { true, CDMBC_TDSTRT, 9 },
		.intr   = { true, IOB_INTREN1, 16 },
	},
	[HSC_DMA_OUT4] = {
		.dma_ch = 20,
		.rb_ch  = 18,
		.td_ch  = 3,
		.en     = { true, CDMBC_TDSTRT, 10 },
		.intr   = { true, IOB_INTREN1, 17 },
	},
	[HSC_DMA_OUT5] = {
		.dma_ch = 21,
		.rb_ch  = 19,
		.td_ch  = 5,
		.en     = { true, CDMBC_TDSTRT, 11 },
		.intr   = { true, IOB_INTREN1, 18 },
	},
	[HSC_DMA_CIP_OUT0] = {
		.dma_ch = 9,
		.rb_ch  = 8,
		.cip_ch = 0,
		.en     = { true, CDMBC_STRT(1), 1 },
		.intr   = { true, IOB_INTREN2, 9 },
	},
	[HSC_DMA_CIP_OUT1] = {
		.dma_ch = 11,
		.rb_ch  = 10,
		.cip_ch = 1,
		.en     = { true, CDMBC_STRT(1), 3 },
		.intr   = { true, IOB_INTREN2, 10 },
	},
};

const struct hsc_spec uniphier_hsc_ld11_spec = {
	.ucode_spu     = { "hsc_spu_code.bin", "hsc_spu_data.bin" },
	.ucode_ace     = { "hsc_ace_code.bin", "hsc_ace_data.bin" },
	.init_rams     = uniphier_hsc_ld11_init_rams,
	.num_init_rams = ARRAY_SIZE(uniphier_hsc_ld11_init_rams),
	.css_in        = uniphier_hsc_ld11_css_in,
	.num_css_in    = ARRAY_SIZE(uniphier_hsc_ld11_css_in),
	.css_out       = uniphier_hsc_ld11_css_out,
	.num_css_out   = ARRAY_SIZE(uniphier_hsc_ld11_css_out),
	.ts_in         = uniphier_hsc_ld11_ts_in,
	.num_ts_in     = ARRAY_SIZE(uniphier_hsc_ld11_ts_in),
	.dma_in        = uniphier_hsc_ld11_dma_in,
	.num_dma_in    = ARRAY_SIZE(uniphier_hsc_ld11_dma_in),
	.dma_out       = uniphier_hsc_ld11_dma_out,
	.num_dma_out   = ARRAY_SIZE(uniphier_hsc_ld11_dma_out),
};

const struct hsc_spec uniphier_hsc_ld20_spec = {
	.ucode_spu     = { "hsc_spu_code.bin", "hsc_spu_data.bin" },
	.ucode_ace     = { "hsc_ace_code.bin", "hsc_ace_data.bin" },
	.init_rams     = uniphier_hsc_ld20_init_rams,
	.num_init_rams = ARRAY_SIZE(uniphier_hsc_ld20_init_rams),
	.css_in        = uniphier_hsc_ld11_css_in,
	.num_css_in    = ARRAY_SIZE(uniphier_hsc_ld11_css_in),
	.css_out       = uniphier_hsc_ld11_css_out,
	.num_css_out   = ARRAY_SIZE(uniphier_hsc_ld11_css_out),
	.ts_in         = uniphier_hsc_ld11_ts_in,
	.num_ts_in     = ARRAY_SIZE(uniphier_hsc_ld11_ts_in),
	.dma_in        = uniphier_hsc_ld11_dma_in,
	.num_dma_in    = ARRAY_SIZE(uniphier_hsc_ld11_dma_in),
	.dma_out       = uniphier_hsc_ld11_dma_out,
	.num_dma_out   = ARRAY_SIZE(uniphier_hsc_ld11_dma_out),
};
