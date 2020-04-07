/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
 *
 * Copyright (c) 2018 Socionext Inc.
 */

#ifndef DVB_UNIPHIER_HSC_H__
#define DVB_UNIPHIER_HSC_H__

#include <linux/bitfield.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <media/dmxdev.h>
#include <media/dvbdev.h>
#include <media/dvb_demux.h>
#include <media/dvb_frontend.h>

enum {
	HSC_CORE_0,
	HSC_CORE_1,
	HSC_CORE_2,
};

enum {
	HSC_UCODE_SPU_0,
	HSC_UCODE_SPU_1,
	HSC_UCODE_ACE,
};

enum {
	HSC_TSIF_MPEG2_TS,
	HSC_TSIF_MPEG2_TS_ATS,
};

/* DPLL */
#define HSC_DPLL0       0
#define HSC_DPLL1       1
#define HSC_DPLL2       2
#define HSC_DPLL3       3

#define HSC_DPLL_NUM    4

/* Clock source of DPLL */
#define HSC_DPLL_SRC_NONE    -1
#define HSC_DPLL_SRC_TSI0    0
#define HSC_DPLL_SRC_TSI1    1
#define HSC_DPLL_SRC_TSI2    2
#define HSC_DPLL_SRC_TSI3    3
#define HSC_DPLL_SRC_TSI4    4
#define HSC_DPLL_SRC_TSI5    5
#define HSC_DPLL_SRC_TSI6    6
#define HSC_DPLL_SRC_TSI7    7
#define HSC_DPLL_SRC_TSI8    8
#define HSC_DPLL_SRC_TSI9    9
#define HSC_DPLL_SRC_REP0    10
#define HSC_DPLL_SRC_REP1    11
#define HSC_DPLL_SRC_REP2    12
#define HSC_DPLL_SRC_REP3    13
#define HSC_DPLL_SRC_REP4    14
#define HSC_DPLL_SRC_REP5    15

#define HSC_DPLL_SRC_NUM     16

/* Port to send to CSS */
#define HSC_CSS_IN_1394_0          0
#define HSC_CSS_IN_1394_1          1
#define HSC_CSS_IN_1394_2          2
#define HSC_CSS_IN_1394_3          3
#define HSC_CSS_IN_DMD0            4
#define HSC_CSS_IN_DMD1            5
#define HSC_CSS_IN_SRLTS0          6
#define HSC_CSS_IN_SRLTS1          7
#define HSC_CSS_IN_SRLTS2          8
#define HSC_CSS_IN_SRLTS3          9
#define HSC_CSS_IN_SRLTS4          10
#define HSC_CSS_IN_SRLTS5          11
#define HSC_CSS_IN_SRLTS6          12
#define HSC_CSS_IN_SRLTS7          13
#define HSC_CSS_IN_PARTS0          16
#define HSC_CSS_IN_PARTS1          17
#define HSC_CSS_IN_PARTS2          18
#define HSC_CSS_IN_PARTS3          19
#define HSC_CSS_IN_TSO0            24
#define HSC_CSS_IN_TSO1            25
#define HSC_CSS_IN_TSO2            26
#define HSC_CSS_IN_TSO3            27
#define HSC_CSS_IN_ENCORDER0_IN    28
#define HSC_CSS_IN_ENCORDER1_IN    29

/* Port to receive from CSS */
#define HSC_CSS_OUT_SRLTS0         0
#define HSC_CSS_OUT_SRLTS1         1
#define HSC_CSS_OUT_SRLTS2         2
#define HSC_CSS_OUT_SRLTS3         3
#define HSC_CSS_OUT_TSI0           4
#define HSC_CSS_OUT_TSI1           5
#define HSC_CSS_OUT_TSI2           6
#define HSC_CSS_OUT_TSI3           7
#define HSC_CSS_OUT_TSI4           8
#define HSC_CSS_OUT_TSI5           9
#define HSC_CSS_OUT_TSI6           10
#define HSC_CSS_OUT_TSI7           11
#define HSC_CSS_OUT_TSI8           12
#define HSC_CSS_OUT_TSI9           13
#define HSC_CSS_OUT_PARTS0         16
#define HSC_CSS_OUT_PARTS1         17
#define HSC_CSS_OUT_PKTFF0         20
#define HSC_CSS_OUT_PKTFF1         21

/* TS input interface */
#define HSC_TS_IN0                 0
#define HSC_TS_IN1                 1
#define HSC_TS_IN2                 2
#define HSC_TS_IN3                 3
#define HSC_TS_IN4                 4
#define HSC_TS_IN5                 5
#define HSC_TS_IN6                 6
#define HSC_TS_IN7                 7
#define HSC_TS_IN8                 8
#define HSC_TS_IN9                 9

/* TS output interface */
#define HSC_TS_OUT0                0
#define HSC_TS_OUT1                1
#define HSC_TS_OUT2                2
#define HSC_TS_OUT3                3
#define HSC_TS_OUT4                4
#define HSC_TS_OUT5                5
#define HSC_TS_OUT6                6
#define HSC_TS_OUT7                7
#define HSC_TS_OUT8                8
#define HSC_TS_OUT9                9

/* DMA to read from memory (Replay DMA) */
#define HSC_DMA_IN0                0
#define HSC_DMA_IN1                1
#define HSC_DMA_IN2                2
#define HSC_DMA_IN3                3
#define HSC_DMA_IN4                4
#define HSC_DMA_IN5                5
#define HSC_DMA_IN6                6
#define HSC_DMA_IN7                7
#define HSC_DMA_IN8                8
#define HSC_DMA_IN9                9
#define HSC_DMA_CIP_IN0            10
#define HSC_DMA_CIP_IN1            11

/* DMA to write to memory (Record DMA) */
#define HSC_DMA_OUT0               0
#define HSC_DMA_OUT1               1
#define HSC_DMA_OUT2               2
#define HSC_DMA_OUT3               3
#define HSC_DMA_OUT4               4
#define HSC_DMA_OUT5               5
#define HSC_DMA_OUT6               6
#define HSC_DMA_OUT7               7
#define HSC_DMA_OUT8               8
#define HSC_DMA_OUT9               9
#define HSC_DMA_CIP_OUT0           10
#define HSC_DMA_CIP_OUT1           11

#define HSC_STREAM_IF_NUM    2

#define HSC_DMAIF_TS_BUFSIZE    (192 * 1024 * 20)

struct hsc_ucode_buf {
	void *buf_code;
	dma_addr_t phys_code;
	size_t size_code;
	void *buf_data;
	dma_addr_t phys_data;
	size_t size_data;
};

struct hsc_spec_ucode {
	const char *name_code;
	const char *name_data;
};

struct hsc_spec_init_ram {
	u32 addr;
	size_t size;
	u32 pattern;
};

struct hsc_reg_cmn {
	int valid;
	u32 reg;
	int sft;
};

struct hsc_reg_css_pol {
	int valid;
	u32 reg;
	int sft_sync;
	int sft_val;
	int sft_clk;
};

struct hsc_reg_css_sel {
	int valid;
	u32 reg;
	u32 mask;
	int sft;
};

struct hsc_spec_css {
	struct hsc_reg_css_pol pol;
	struct hsc_reg_css_sel sel;
};

struct hsc_spec_ts {
	struct hsc_reg_cmn intr;
};

struct hsc_spec_dma {
	/* DMA channel for CDMBC_CH* registers */
	int dma_ch;
	/* Ring buffer channel for CDMBC_RB* registers */
	int rb_ch;
	/* CIP file channel for CDMBC_CIP* registers */
	int cip_ch;
	/* Intermit transfer channel for CDMBC_IT* registers */
	int it_ch;
	/* DMA channel (output only) for CDMBC_CHTDCTR* registers */
	int td_ch;
	struct hsc_reg_cmn en;
	struct hsc_reg_cmn intr;

};

struct hsc_spec {
	struct hsc_spec_ucode ucode_spu;
	struct hsc_spec_ucode ucode_ace;
	const struct hsc_spec_init_ram *init_rams;
	size_t num_init_rams;
	const struct hsc_spec_css *css_in;
	size_t num_css_in;
	const struct hsc_spec_css *css_out;
	size_t num_css_out;
	const struct hsc_spec_ts *ts_in;
	size_t num_ts_in;
	const struct hsc_spec_dma *dma_in;
	size_t num_dma_in;
	const struct hsc_spec_dma *dma_out;
	size_t num_dma_out;
};

struct hsc_tsif {
	struct hsc_chip *chip;

	struct dvb_adapter adapter;
	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dvb_frontend *fe;
	int valid_adapter;
	int valid_demux;
	int valid_dmxdev;
	int valid_fe;

	int css_in;
	int css_out;
	int tsi;
	int dpll;
	int dpll_src;
	struct hsc_dmaif *dmaif;

	int running;
	struct delayed_work recover_work;
	unsigned long recover_delay;
};

struct hsc_dma {
	struct hsc_chip *chip;

	int id;
	const struct hsc_spec_dma *spec;
	struct hsc_dma_buf *buf;
};

struct hsc_dma_buf {
	void *virt;
	dma_addr_t phys;
	u64 size;
	u64 size_chk;
	u64 rd_offs;
	u64 wr_offs;
	u64 chk_offs;
};

struct hsc_dmaif {
	struct hsc_chip *chip;

	struct hsc_dma_buf buf_out;
	struct hsc_dma dma_out;

	struct hsc_tsif *tsif;

	/* guard read/write pointer of DMA buffer from interrupt handler */
	spinlock_t lock;
	int running;
	struct work_struct feed_work;
};

struct hsc_chip {
	const struct hsc_spec *spec;
	short *adapter_nums;

	struct platform_device *pdev;
	struct regmap *regmap;
	struct clk *clk_stdmac;
	struct clk *clk_hsc;
	struct reset_control *rst_stdmac;
	struct reset_control *rst_hsc;

	struct hsc_dmaif dmaif[HSC_STREAM_IF_NUM];
	struct hsc_tsif tsif[HSC_STREAM_IF_NUM];

	struct hsc_ucode_buf ucode_spu;
	struct hsc_ucode_buf ucode_am;
};

struct hsc_conf {
	int css_in;
	int css_out;
	int dpll;
	int dma_out;
};

/* CSS */
int hsc_css_out_to_ts_in(int out);
int hsc_css_out_to_dpll_src(int out);

int hsc_dpll_get_src(struct hsc_chip *chip, int dpll, int *src);
int hsc_dpll_set_src(struct hsc_chip *chip, int dpll, int src);
int hsc_css_in_get_polarity(struct hsc_chip *chip, int in,
			    bool *sync_bit, bool *val_bit, bool *clk_fall);
int hsc_css_in_set_polarity(struct hsc_chip *chip, int in,
			    bool sync_bit, bool val_bit, bool clk_fall);
int hsc_css_out_get_polarity(struct hsc_chip *chip, int out,
			     bool *sync_bit, bool *val_bit, bool *clk_fall);
int hsc_css_out_set_polarity(struct hsc_chip *chip, int out,
			     bool sync_bit, bool val_bit, bool clk_fall);
int hsc_css_out_get_src(struct hsc_chip *chip, int *in, int out, bool *en);
int hsc_css_out_set_src(struct hsc_chip *chip, int in, int out, bool en);

/* TS */
int hsc_ts_in_set_enable(struct hsc_chip *chip, int tsi, bool en);
int hsc_ts_in_set_dmaparam(struct hsc_chip *chip, int tsi, int ifmt);
int hsc_ts_in_get_intr(struct hsc_chip *chip, int tsi, u32 *st);
void hsc_ts_in_clear_intr(struct hsc_chip *chip, int tsi, u32 clear);

/* DMA */
u64 hsc_rb_cnt(struct hsc_dma_buf *buf);
u64 hsc_rb_cnt_to_end(struct hsc_dma_buf *buf);
u64 hsc_rb_space(struct hsc_dma_buf *buf);
u64 hsc_rb_space_to_end(struct hsc_dma_buf *buf);

void hsc_dma_rb_set_buffer(struct hsc_chip *chip, int rb_ch, u64 bg, u64 ed);
u64 hsc_dma_rb_get_rp(struct hsc_chip *chip, int rb_ch);
void hsc_dma_rb_set_rp(struct hsc_chip *chip, int rb_ch, u64 pos);
u64 hsc_dma_rb_get_wp(struct hsc_chip *chip, int rb_ch);
void hsc_dma_rb_set_wp(struct hsc_chip *chip, int rb_ch, u64 pos);

int hsc_dma_out_init(struct hsc_dma *dma_out, struct hsc_chip *chip,
		     int id, struct hsc_dma_buf *buf);
void hsc_dma_out_set_src_ts_in(struct hsc_dma *dma_out, int tsi);
void hsc_dma_out_start(struct hsc_dma *dma_out, bool en);
void hsc_dma_out_sync(struct hsc_dma *dma_out);
int hsc_dma_out_get_intr(struct hsc_dma *dma_out, u32 *stat);
void hsc_dma_out_clear_intr(struct hsc_dma *dma_out, u32 clear);

/* UCODE DL */
int hsc_ucode_load_all(struct hsc_chip *chip);
int hsc_ucode_unload_all(struct hsc_chip *chip);

/* For Adapter */
int hsc_register_dvb(struct hsc_tsif *tsif);
void hsc_unregister_dvb(struct hsc_tsif *tsif);
int hsc_tsif_init(struct hsc_tsif *tsif, const struct hsc_conf *conf);
void hsc_tsif_release(struct hsc_tsif *tsif);
int hsc_dmaif_init(struct hsc_dmaif *dmaif, const struct hsc_conf *conf);
void hsc_dmaif_release(struct hsc_dmaif *dmaif);
extern const struct hsc_spec uniphier_hsc_ld11_spec;
extern const struct hsc_spec uniphier_hsc_ld20_spec;

#endif /* DVB_UNIPHIER_HSC_H__ */
