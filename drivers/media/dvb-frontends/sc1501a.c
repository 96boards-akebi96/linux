// SPDX-License-Identifier: GPL-2.0
//
// Socionext SC1501A series demodulator driver for ISDB-S/ISDB-T.
//
// Copyright (c) 2018 Socionext Inc.

//#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <dvb_math.h>

#include "sc1501a.h"

/* copy from include/linux/bitfield.h */

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define __BF_FIELD_CHECK(_mask, _reg, _val, _pfx)			\
	({								\
		BUILD_BUG_ON_MSG(!__builtin_constant_p(_mask),		\
				 _pfx "mask is not constant");		\
		BUILD_BUG_ON_MSG(!(_mask), _pfx "mask is zero");	\
		BUILD_BUG_ON_MSG(__builtin_constant_p(_val) ?		\
				 ~((_mask) >> __bf_shf(_mask)) & (_val) : 0, \
				 _pfx "value too large for the field"); \
		BUILD_BUG_ON_MSG((_mask) > (typeof(_reg))~0ull,		\
				 _pfx "type of reg too small for mask"); \
		0; \
	})

/**
 * FIELD_FIT() - check if value fits in the field
 * @_mask: shifted mask defining the field's length and position
 * @_val:  value to test against the field
 *
 * Return: true if @_val can fit inside @_mask, false if @_val is too big.
 */
#define FIELD_FIT(_mask, _val)						\
	({								\
		__BF_FIELD_CHECK(_mask, 0ULL, _val, "FIELD_FIT: ");	\
		!((((typeof(_mask))_val) << __bf_shf(_mask)) & ~(_mask)); \
	})

/**
 * FIELD_PREP() - prepare a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_val:  value to put in the field
 *
 * FIELD_PREP() masks and shifts up the value.  The result should
 * be combined with other fields of the bitfield using logical OR.
 */
#define FIELD_PREP(_mask, _val)						\
	({								\
		__BF_FIELD_CHECK(_mask, 0ULL, _val, "FIELD_PREP: ");	\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})

/* ISDB-S registers */
#define ATSIDU_S                                    0x2f
#define ATSIDL_S                                    0x30
#define TSSET_S                                     0x31
#define AGCREAD_S                                   0x5a
#define CPMON1_S                                    0x5e
#define   CPMON1_S_FSYNC                              BIT(5)
#define   CPMON1_S_ERRMON                             BIT(4)
#define   CPMON1_S_SIGOFF                             BIT(3)
#define   CPMON1_S_W2LOCK                             BIT(2)
#define   CPMON1_S_W1LOCK                             BIT(1)
#define   CPMON1_S_DW1LOCK                            BIT(0)
#define TRMON_S                                     0x60
#define BERCNFLG_S                                  0x68
#define   BERCNFLG_S_BERVRDY                          BIT(5)
#define   BERCNFLG_S_BERVCHK                          BIT(4)
#define   BERCNFLG_S_BERDRDY                          BIT(3)
#define   BERCNFLG_S_BERDCHK                          BIT(2)
#define CNRDXU_S                                    0x69
#define CNRDXL_S                                    0x6a
#define CNRDYU_S                                    0x6b
#define CNRDYL_S                                    0x6c
#define BERVRDU_S                                   0x71
#define BERVRDL_S                                   0x72
#define DOSET1_S                                    0x73

/* Primary ISDB-T */
#define PLLASET1                                    0x00
#define PLLASET2                                    0x01
#define PLLBSET1                                    0x02
#define PLLBSET2                                    0x03
#define PLLSET                                      0x04
#define OUTCSET                                     0x08
#define   OUTCSET_CHDRV_8MA                           0xff
#define   OUTCSET_CHDRV_4MA                           0x00
#define PLDWSET                                     0x09
#define   PLDWSET_NORMAL                             0x00
#define   PLDWSET_PULLDOWN                           0xff
#define HIZSET1                                     0x0a
#define HIZSET2                                     0x0b

/* Secondary ISDB-T (for MN884434 only) */
#define RCVSET                                      0x00
#define TSSET1_M                                    0x01
#define TSSET2_M                                    0x02
#define TSSET3_M                                    0x03
#define INTACSET                                    0x08
#define HIZSET3                                     0x0b

/* ISDB-T registers */
#define TSSET1                                      0x05
#define   TSSET1_TSASEL_MASK                          GENMASK(4, 3)
#define   TSSET1_TSASEL_ISDBT                         (0x0 << 3)
#define   TSSET1_TSASEL_ISDBS                         (0x1 << 3)
#define   TSSET1_TSASEL_NONE                          (0x2 << 3)
#define   TSSET1_TSBSEL_MASK                          GENMASK(2, 1)
#define   TSSET1_TSBSEL_ISDBS                         (0x0 << 1)
#define   TSSET1_TSBSEL_ISDBT                         (0x1 << 1)
#define   TSSET1_TSBSEL_NONE                          (0x2 << 1)
#define TSSET2                                      0x06
#define TSSET3                                      0x07
#define   TSSET3_INTASEL_MASK                         GENMASK(7, 6)
#define   TSSET3_INTASEL_T                            (0x0 << 6)
#define   TSSET3_INTASEL_S                            (0x1 << 6)
#define   TSSET3_INTASEL_NONE                         (0x2 << 6)
#define   TSSET3_INTBSEL_MASK                         GENMASK(5, 4)
#define   TSSET3_INTBSEL_S                            (0x0 << 4)
#define   TSSET3_INTBSEL_T                            (0x1 << 4)
#define   TSSET3_INTBSEL_NONE                         (0x2 << 4)
#define OUTSET2                                     0x0d
#define PWDSET                                      0x0f
#define   PWDSET_OFDMPD_MASK                          GENMASK(3, 2)
#define   PWDSET_OFDMPD_DOWN                          BIT(3)
#define   PWDSET_PSKPD_MASK                           GENMASK(1, 0)
#define   PWDSET_PSKPD_DOWN                           BIT(1)
#define CLKSET1_T                                   0x11
#define MDSET_T                                     0x13
#define   MDSET_T_MDAUTO_MASK                         GENMASK(7, 4)
#define   MDSET_T_MDAUTO_AUTO                         (0xf << 4)
#define   MDSET_T_MDAUTO_MANUAL                       (0x0 << 4)
#define   MDSET_T_FFTS_MASK                           GENMASK(3, 2)
#define   MDSET_T_FFTS_MODE1                          (0x0 << 2)
#define   MDSET_T_FFTS_MODE2                          (0x1 << 2)
#define   MDSET_T_FFTS_MODE3                          (0x2 << 2)
#define   MDSET_T_GI_MASK                             GENMASK(1, 0)
#define   MDSET_T_GI_1_32                             (0x0 << 0)
#define   MDSET_T_GI_1_16                             (0x1 << 0)
#define   MDSET_T_GI_1_8                              (0x2 << 0)
#define   MDSET_T_GI_1_4                              (0x3 << 0)
#define MDASET_T                                    0x14
#define ADCSET1_T                                   0x20
#define   ADCSET1_T_REFSEL_MASK                       GENMASK(1, 0)
#define   ADCSET1_T_REFSEL_2V                         (0x3 << 0)
#define   ADCSET1_T_REFSEL_1_5V                       (0x2 << 0)
#define   ADCSET1_T_REFSEL_1V                         (0x1 << 0)
#define NCOFREQU_T                                  0x24
#define NCOFREQM_T                                  0x25
#define NCOFREQL_T                                  0x26
#define FADU_T                                      0x27
#define FADM_T                                      0x28
#define FADL_T                                      0x29
#define AGCSET2_T                                   0x2c
#define   AGCSET2_T_IFPOLINV_INC                      BIT(0)
#define   AGCSET2_T_RFPOLINV_INC                      BIT(1)
#define AGCV3_T                                     0x3e
#define MDRD_T                                      0xa2
#define   MDRD_T_SEGID_MASK                           GENMASK(5, 4)
#define   MDRD_T_SEGID_13                             (0x0 << 4)
#define   MDRD_T_SEGID_1                              (0x1 << 4)
#define   MDRD_T_SEGID_3                              (0x2 << 4)
#define   MDRD_T_FFTS_MASK                            GENMASK(3, 2)
#define   MDRD_T_FFTS_MODE1                           (0x0 << 2)
#define   MDRD_T_FFTS_MODE2                           (0x1 << 2)
#define   MDRD_T_FFTS_MODE3                           (0x2 << 2)
#define   MDRD_T_GI_MASK                              GENMASK(1, 0)
#define   MDRD_T_GI_1_32                              (0x0 << 0)
#define   MDRD_T_GI_1_16                              (0x1 << 0)
#define   MDRD_T_GI_1_8                               (0x2 << 0)
#define   MDRD_T_GI_1_4                               (0x3 << 0)
#define SSEQRD_T                                    0xa3
#define   SSEQRD_T_SSEQSTRD_MASK                      GENMASK(3, 0)
#define   SSEQRD_T_SSEQSTRD_RESET                     (0x0 << 0)
#define   SSEQRD_T_SSEQSTRD_TUNING                    (0x1 << 0)
#define   SSEQRD_T_SSEQSTRD_AGC                       (0x2 << 0)
#define   SSEQRD_T_SSEQSTRD_SEARCH                    (0x3 << 0)
#define   SSEQRD_T_SSEQSTRD_CLOCK_SYNC                (0x4 << 0)
#define   SSEQRD_T_SSEQSTRD_FREQ_SYNC                 (0x8 << 0)
#define   SSEQRD_T_SSEQSTRD_FRAME_SYNC                (0x9 << 0)
#define   SSEQRD_T_SSEQSTRD_SYNC                      (0xa << 0)
#define   SSEQRD_T_SSEQSTRD_LOCK                      (0xb << 0)
#define AGCRDU_T                                    0xa8
#define AGCRDL_T                                    0xa9
#define CNRDU_T                                     0xbe
#define CNRDL_T                                     0xbf
#define BERFLG_T                                    0xc0
#define   BERFLG_T_BERDRDY                            BIT(7)
#define   BERFLG_T_BERDCHK                            BIT(6)
#define   BERFLG_T_BERVRDYA                           BIT(5)
#define   BERFLG_T_BERVCHKA                           BIT(4)
#define   BERFLG_T_BERVRDYB                           BIT(3)
#define   BERFLG_T_BERVCHKB                           BIT(2)
#define   BERFLG_T_BERVRDYC                           BIT(1)
#define   BERFLG_T_BERVCHKC                           BIT(0)
#define BERRDU_T                                    0xc1
#define BERRDM_T                                    0xc2
#define BERRDL_T                                    0xc3
#define BERLENRDU_T                                 0xc4
#define BERLENRDL_T                                 0xc5
#define ERRFLG_T                                    0xc6
#define   ERRFLG_T_BERDOVF                            BIT(7)
#define   ERRFLG_T_BERVOVFA                           BIT(6)
#define   ERRFLG_T_BERVOVFB                           BIT(5)
#define   ERRFLG_T_BERVOVFC                           BIT(4)
#define   ERRFLG_T_NERRFA                             BIT(3)
#define   ERRFLG_T_NERRFB                             BIT(2)
#define   ERRFLG_T_NERRFC                             BIT(1)
#define   ERRFLG_T_NERRF                              BIT(0)
#define DOSET1_T                                    0xcf

#define CLK_LOW            4000000
#define CLK_DIRECT         20200000
#define CLK_MAX            25410000

#define S_T_FREQ           8126984 /* 512 / 63 MHz */

struct sc1501a_spec {
	bool primary;
};

struct sc1501a_priv {
	const struct sc1501a_spec *spec;

	struct dvb_frontend fe;
	struct clk *mclk;
	struct gpio_desc *reset_gpio;
	u32 clk_freq;
	u32 if_freq;

	/* Common */
	bool use_clkbuf;

	/* ISDB-S */
	struct i2c_client *client_s;
	struct regmap *regmap_s;

	/* ISDB-T */
	struct i2c_client *client_t;
	struct regmap *regmap_t;
};

static void sc1501a_cmn_power_on(struct sc1501a_priv *chip)
{
	struct regmap *r_t = chip->regmap_t;

	clk_prepare_enable(chip->mclk);

	gpiod_set_value_cansleep(chip->reset_gpio, 1);
	usleep_range(100, 1000);
	gpiod_set_value_cansleep(chip->reset_gpio, 0);

	if (chip->spec->primary) {
		regmap_write(r_t, OUTCSET, OUTCSET_CHDRV_8MA);
		regmap_write(r_t, PLDWSET, PLDWSET_NORMAL);
		regmap_write(r_t, HIZSET1, 0x80);
		regmap_write(r_t, HIZSET2, 0xe0);
	} else {
		regmap_write(r_t, HIZSET3, 0x8f);
	}
}

static void sc1501a_cmn_power_off(struct sc1501a_priv *chip)
{
	gpiod_set_value_cansleep(chip->reset_gpio, 1);

	clk_disable_unprepare(chip->mclk);
}

static void sc1501a_s_sleep(struct sc1501a_priv *chip)
{
	struct regmap *r_t = chip->regmap_t;

	regmap_update_bits(r_t, PWDSET, PWDSET_PSKPD_MASK,
			   PWDSET_PSKPD_DOWN);
}

static void sc1501a_s_wake(struct sc1501a_priv *chip)
{
	struct regmap *r_t = chip->regmap_t;

	regmap_update_bits(r_t, PWDSET, PWDSET_PSKPD_MASK, 0);
}

static void sc1501a_s_tune(struct sc1501a_priv *chip,
			   struct dtv_frontend_properties *c)
{
	struct regmap *r_s = chip->regmap_s;

	regmap_write(r_s, ATSIDU_S, c->stream_id >> 8);
	regmap_write(r_s, ATSIDL_S, c->stream_id);
	regmap_write(r_s, TSSET_S, 0);
}

static int sc1501a_s_read_status(struct sc1501a_priv *chip,
				 struct dtv_frontend_properties *c,
				 enum fe_status *status)
{
	struct regmap *r_s = chip->regmap_s;
	u32 cpmon, tmpu, tmpl, flg;
	u64 tmp;

	/* Sync detection */
	regmap_read(r_s, CPMON1_S, &cpmon);

	*status = 0;
	if (cpmon & CPMON1_S_FSYNC)
		*status |= FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
	if (cpmon & CPMON1_S_W2LOCK)
		*status |= FE_HAS_SIGNAL | FE_HAS_CARRIER;

	/* Signal strength */
	c->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	if (*status & FE_HAS_SIGNAL) {
		u32 agc;

		regmap_read(r_s, AGCREAD_S, &tmpu);
		agc = tmpu << 8;

		c->strength.len = 1;
		c->strength.stat[0].scale = FE_SCALE_RELATIVE;
		c->strength.stat[0].uvalue = agc;
	}

	/* C/N rate */
	c->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	if (*status & FE_HAS_VITERBI) {
		u32 cnr = 0, x, y, d;
		u64 d_3 = 0;

		regmap_read(r_s, CNRDXU_S, &tmpu);
		regmap_read(r_s, CNRDXL_S, &tmpl);
		x = (tmpu << 8) | tmpl;
		regmap_read(r_s, CNRDYU_S, &tmpu);
		regmap_read(r_s, CNRDYL_S, &tmpl);
		y = (tmpu << 8) | tmpl;

		/* CNR[dB]: 10 * log10(D) - 30.74 / D^3 - 3 */
		/*   D = x^2 / (2^15 * y - x^2) */
		d = (y << 15) - x * x;
		if (d > 0) {
			/* (2^4 * D)^3 = 2^12 * D^3 */
			/* 3.074 * 2^(12 + 24) = 211243671486 */
			d_3 = div_u64(16 * x * x, d);
			d_3 = d_3 * d_3 * d_3;
			if (d_3)
				d_3 = div_u64(211243671486ULL, d_3);
		}

		if (d_3) {
			/* 0.3 * 2^24 = 5033164 */
			tmp = (s64)2 * intlog10(x) - intlog10(abs(d)) - d_3
				- 5033164;
			cnr = div_u64(tmp * 10000, 1 << 24);
		}

		if (cnr) {
			c->cnr.len = 1;
			c->cnr.stat[0].scale = FE_SCALE_DECIBEL;
			c->cnr.stat[0].uvalue = cnr;
		}
	}

	/* BER */
	c->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	c->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	regmap_read(r_s, BERCNFLG_S, &flg);

	if ((*status & FE_HAS_VITERBI) && (flg & BERCNFLG_S_BERVRDY)) {
		u32 bit_err, bit_cnt;

		regmap_read(r_s, BERVRDU_S, &tmpu);
		regmap_read(r_s, BERVRDL_S, &tmpl);
		bit_err = (tmpu << 8) | tmpl;
		bit_cnt = (1 << 13) * 204;

		if (bit_cnt) {
			c->post_bit_error.len = 1;
			c->post_bit_error.stat[0].scale = FE_SCALE_COUNTER;
			c->post_bit_error.stat[0].uvalue = bit_err;
			c->post_bit_count.len = 1;
			c->post_bit_count.stat[0].scale = FE_SCALE_COUNTER;
			c->post_bit_count.stat[0].uvalue = bit_cnt;
		}
	}

	return 0;
}

static void sc1501a_t_sleep(struct sc1501a_priv *chip)
{
	struct regmap *r_t = chip->regmap_t;

	regmap_update_bits(r_t, PWDSET, PWDSET_OFDMPD_MASK,
			   PWDSET_OFDMPD_DOWN);
}

static void sc1501a_t_wake(struct sc1501a_priv *chip)
{
	struct regmap *r_t = chip->regmap_t;

	regmap_update_bits(r_t, PWDSET, PWDSET_OFDMPD_MASK, 0);
}

static bool sc1501a_t_is_valid_clk(u32 adckt, u32 if_freq)
{
	if (if_freq == DIRECT_IF_57MHZ) {
		if (adckt >= CLK_DIRECT && adckt <= 21000000)
			return true;
		if (adckt >= 25300000 && adckt <= CLK_MAX)
			return true;
	} else if (if_freq == DIRECT_IF_44MHZ) {
		if (adckt >= 25000000 && adckt <= CLK_MAX)
			return true;
	} else if (if_freq >= LOW_IF_4MHZ && if_freq < DIRECT_IF_44MHZ) {
		if (adckt >= CLK_DIRECT && adckt <= CLK_MAX)
			return true;
	}

	return false;
}

static int sc1501a_t_set_freq(struct sc1501a_priv *chip)
{
	struct device *dev = &chip->client_s->dev;
	struct regmap *r_t = chip->regmap_t;
	s64 adckt, nco, ad_t;
	u32 m, v;

	/* Clock buffer (but not supported) or XTAL */
	if (chip->clk_freq >= CLK_LOW && chip->clk_freq < CLK_DIRECT) {
		chip->use_clkbuf = true;
		regmap_write(r_t, CLKSET1_T, 0x07);

		adckt = 0;
	} else {
		chip->use_clkbuf = false;
		regmap_write(r_t, CLKSET1_T, 0x00);

		adckt = chip->clk_freq;
	}
	if (!sc1501a_t_is_valid_clk(adckt, chip->if_freq)) {
		dev_err(dev, "Invalid clock, CLK:%d, ADCKT:%lld, IF:%d\n",
			chip->clk_freq, adckt, chip->if_freq);
		return -EINVAL;
	}

	/* Direct IF or Low IF */
	if (chip->if_freq == DIRECT_IF_57MHZ ||
	    chip->if_freq == DIRECT_IF_44MHZ)
		nco = adckt * 2 - chip->if_freq;
	else
		nco = -((s64)chip->if_freq);
	nco = div_s64(nco << 24, adckt);
	ad_t = div_s64(adckt << 22, S_T_FREQ);

	regmap_write(r_t, NCOFREQU_T, nco >> 16);
	regmap_write(r_t, NCOFREQM_T, nco >> 8);
	regmap_write(r_t, NCOFREQL_T, nco);
	regmap_write(r_t, FADU_T, ad_t >> 16);
	regmap_write(r_t, FADM_T, ad_t >> 8);
	regmap_write(r_t, FADL_T, ad_t);

	/* Level of IF */
	m = ADCSET1_T_REFSEL_MASK;
	v = ADCSET1_T_REFSEL_1_5V;
	regmap_update_bits(r_t, ADCSET1_T, m, v);

	/* Polarity of AGC */
	v = AGCSET2_T_IFPOLINV_INC | AGCSET2_T_RFPOLINV_INC;
	regmap_update_bits(r_t, AGCSET2_T, v, v);

	/* Lower output level of AGC */
	regmap_write(r_t, AGCV3_T, 0x00);

	regmap_write(r_t, MDSET_T, 0xfa);

	return 0;
}

static void sc1501a_t_tune(struct sc1501a_priv *chip,
			   struct dtv_frontend_properties *c)
{
	struct regmap *r_t = chip->regmap_t;
	u32 m, v;

	m = MDSET_T_MDAUTO_MASK | MDSET_T_FFTS_MASK | MDSET_T_GI_MASK;
	v = MDSET_T_MDAUTO_AUTO | MDSET_T_FFTS_MODE3 | MDSET_T_GI_1_8;
	regmap_update_bits(r_t, MDSET_T, m, v);

	regmap_write(r_t, MDASET_T, 0);
}

static int sc1501a_t_read_status(struct sc1501a_priv *chip,
				 struct dtv_frontend_properties *c,
				 enum fe_status *status)
{
	struct regmap *r_t = chip->regmap_t;
	u32 seqrd, st, flg, tmpu, tmpm, tmpl;
	u64 tmp;

	/* Sync detection */
	regmap_read(r_t, SSEQRD_T, &seqrd);
	st = seqrd & SSEQRD_T_SSEQSTRD_MASK;

	*status = 0;
	if (st >= SSEQRD_T_SSEQSTRD_SYNC)
		*status |= FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
	if (st >= SSEQRD_T_SSEQSTRD_FRAME_SYNC)
		*status |= FE_HAS_SIGNAL | FE_HAS_CARRIER;

	/* Signal strength */
	c->strength.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	if (*status & FE_HAS_SIGNAL) {
		u32 agc;

		regmap_read(r_t, AGCRDU_T, &tmpu);
		regmap_read(r_t, AGCRDL_T, &tmpl);
		agc = (tmpu << 8) | tmpl;

		c->strength.len = 1;
		c->strength.stat[0].scale = FE_SCALE_RELATIVE;
		c->strength.stat[0].uvalue = agc;
	}

	/* C/N rate */
	c->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	if (*status & FE_HAS_VITERBI) {
		u32 cnr;

		regmap_read(r_t, CNRDU_T, &tmpu);
		regmap_read(r_t, CNRDL_T, &tmpl);

		if (tmpu || tmpl) {
			/* CNR[dB]: 10 * (log10(65536 / value) + 0.2) */
			/* intlog10(65536) = 80807124, 0.2 * 2^24 = 3355443 */
			tmp = (u64)80807124 - intlog10((tmpu << 8) | tmpl)
				+ 3355443;
			cnr = div_u64(tmp * 10000, 1 << 24);
		} else {
			cnr = 0;
		}

		c->cnr.len = 1;
		c->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		c->cnr.stat[0].uvalue = cnr;
	}

	/* BER */
	c->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	c->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	regmap_read(r_t, BERFLG_T, &flg);

	if ((*status & FE_HAS_VITERBI) && (flg & BERFLG_T_BERVRDYA)) {
		u32 bit_err, bit_cnt;

		regmap_read(r_t, BERRDU_T, &tmpu);
		regmap_read(r_t, BERRDM_T, &tmpm);
		regmap_read(r_t, BERRDL_T, &tmpl);
		bit_err = (tmpu << 16) | (tmpm << 8) | tmpl;

		regmap_read(r_t, BERLENRDU_T, &tmpu);
		regmap_read(r_t, BERLENRDL_T, &tmpl);
		bit_cnt = ((tmpu << 8) | tmpl) * 203 * 8;

		if (bit_cnt) {
			c->post_bit_error.len = 1;
			c->post_bit_error.stat[0].scale = FE_SCALE_COUNTER;
			c->post_bit_error.stat[0].uvalue = bit_err;
			c->post_bit_count.len = 1;
			c->post_bit_count.stat[0].scale = FE_SCALE_COUNTER;
			c->post_bit_count.stat[0].uvalue = bit_cnt;
		}
	}

	return 0;
}

static int sc1501a_sleep(struct dvb_frontend *fe)
{
	struct sc1501a_priv *chip = fe->demodulator_priv;

	sc1501a_s_sleep(chip);
	sc1501a_t_sleep(chip);

	return 0;
}

static int sc1501a_set_frontend(struct dvb_frontend *fe)
{
	struct sc1501a_priv *chip = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct regmap *r_s = chip->regmap_s;
	struct regmap *r_t = chip->regmap_t;
	u8 tssel = 0, intsel = 0;

	if (c->delivery_system == SYS_ISDBS) {
		sc1501a_s_wake(chip);
		sc1501a_t_sleep(chip);

		tssel = TSSET1_TSASEL_ISDBS;
		intsel = TSSET3_INTASEL_S;
	} else if (c->delivery_system == SYS_ISDBT) {
		sc1501a_s_sleep(chip);
		sc1501a_t_wake(chip);

		sc1501a_t_set_freq(chip);

		tssel = TSSET1_TSASEL_ISDBT;
		intsel = TSSET3_INTASEL_T;
	}

	regmap_update_bits(r_t, TSSET1,
			   TSSET1_TSASEL_MASK | TSSET1_TSBSEL_MASK,
			   tssel | TSSET1_TSBSEL_NONE);
	regmap_write(r_t, TSSET2, 0);
	regmap_update_bits(r_t, TSSET3,
			   TSSET3_INTASEL_MASK | TSSET3_INTBSEL_MASK,
			   intsel | TSSET3_INTBSEL_NONE);

	regmap_write(r_t, DOSET1_T, 0x95);
	regmap_write(r_s, DOSET1_S, 0x80);

	if (c->delivery_system == SYS_ISDBS)
		sc1501a_s_tune(chip, c);
	else if (c->delivery_system == SYS_ISDBT)
		sc1501a_t_tune(chip, c);

	if (fe->ops.tuner_ops.set_params) {
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 1);
		fe->ops.tuner_ops.set_params(fe);
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 0);
	}

	return 0;
}

static int sc1501a_get_tune_settings(struct dvb_frontend *fe,
				     struct dvb_frontend_tune_settings *s)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	s->min_delay_ms = 850;

	if (c->delivery_system == SYS_ISDBS) {
		s->max_drift = 30000 * 2 + 1;
		s->step_size = 30000;
	} else if (c->delivery_system == SYS_ISDBT) {
		s->max_drift = 142857 * 2 + 1;
		s->step_size = 142857 * 2;
	}

	return 0;
}

static int sc1501a_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	struct sc1501a_priv *chip = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (c->delivery_system == SYS_ISDBS)
		return sc1501a_s_read_status(chip, c, status);

	if (c->delivery_system == SYS_ISDBT)
		return sc1501a_t_read_status(chip, c, status);

	return -EINVAL;
}

static const struct dvb_frontend_ops sc1501a_ops = {
	.delsys = { SYS_ISDBS, SYS_ISDBT },
	.info = {
		.name          = "Socionext SC1501A",
		.frequency_min = 1032000,
		.frequency_max = 770000000,
		.caps = FE_CAN_INVERSION_AUTO | FE_CAN_FEC_AUTO |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO,
	},

	.sleep                   = sc1501a_sleep,
	.set_frontend            = sc1501a_set_frontend,
	.get_tune_settings       = sc1501a_get_tune_settings,
	.read_status             = sc1501a_read_status,
};

static const struct regmap_config regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 8,
	.cache_type = REGCACHE_NONE,
};

static int sc1501a_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct sc1501a_config *conf = client->dev.platform_data;
	struct sc1501a_priv *chip;
	struct device *dev = &client->dev;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (dev->of_node)
		chip->spec = of_device_get_match_data(dev);
	else
		chip->spec = (struct sc1501a_spec *)id->driver_data;
	if (!chip->spec)
		return -EINVAL;

	chip->mclk = devm_clk_get(dev, "mclk");
	if (IS_ERR(chip->mclk) && !conf) {
		dev_err(dev, "Failed to request mclk: %ld\n",
			PTR_ERR(chip->mclk));
		return PTR_ERR(chip->mclk);
	}

	ret = of_property_read_u32(dev->of_node, "if-frequency",
				   &chip->if_freq);
	if (ret && !conf) {
		dev_err(dev, "Failed to load IF frequency: %d.\n", ret);
		return ret;
	}

	chip->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						   GPIOD_OUT_HIGH);
	if (IS_ERR(chip->reset_gpio)) {
		dev_err(dev, "Failed to request reset_gpio: %ld\n",
			PTR_ERR(chip->reset_gpio));
		return PTR_ERR(chip->reset_gpio);
	}

	if (conf) {
		chip->mclk = conf->mclk;
		chip->if_freq = conf->if_freq;
		chip->reset_gpio = conf->reset_gpio;

		*conf->fe = &chip->fe;
	}

	chip->client_s = client;
	chip->regmap_s = devm_regmap_init_i2c(chip->client_s, &regmap_config);
	if (IS_ERR(chip->regmap_s))
		return PTR_ERR(chip->regmap_s);

	/*
	 * Chip has two I2C addresses for each satellite/terrestrial system.
	 * ISDB-T uses address ISDB-S + 4, so we register a dummy client.
	 */
	chip->client_t = i2c_new_dummy(client->adapter, client->addr + 4);
	if (!chip->client_t)
		return -ENODEV;

	chip->regmap_t = devm_regmap_init_i2c(chip->client_t, &regmap_config);
	if (IS_ERR(chip->regmap_t)) {
		ret = PTR_ERR(chip->regmap_t);
		goto err_i2c_t;
	}

	chip->clk_freq = clk_get_rate(chip->mclk);

	memcpy(&chip->fe.ops, &sc1501a_ops, sizeof(sc1501a_ops));
	chip->fe.demodulator_priv = chip;
	i2c_set_clientdata(client, chip);

	sc1501a_cmn_power_on(chip);
	sc1501a_s_sleep(chip);
	sc1501a_t_sleep(chip);

	return 0;

err_i2c_t:
	i2c_unregister_device(chip->client_t);

	return ret;
}

static int sc1501a_remove(struct i2c_client *client)
{
	struct sc1501a_priv *chip = i2c_get_clientdata(client);

	sc1501a_cmn_power_off(chip);

	i2c_unregister_device(chip->client_t);

	return 0;
}

static const struct sc1501a_spec sc1501a_spec_pri = {
	.primary = true,
};

static const struct sc1501a_spec sc1501a_spec_sec = {
	.primary = false,
};

static const struct of_device_id sc1501a_of_match[] = {
	{ .compatible = "socionext,mn884433",   .data = &sc1501a_spec_pri, },
	{ .compatible = "socionext,mn884434-0", .data = &sc1501a_spec_pri, },
	{ .compatible = "socionext,mn884434-1", .data = &sc1501a_spec_sec, },
	{ .compatible = "socionext,sc1501a",    .data = &sc1501a_spec_pri, },
	{}
};
MODULE_DEVICE_TABLE(of, sc1501a_of_match);

static const struct i2c_device_id sc1501a_i2c_id[] = {
	{ "mn884433",   (kernel_ulong_t)&sc1501a_spec_pri },
	{ "mn884434-0", (kernel_ulong_t)&sc1501a_spec_pri },
	{ "mn884434-1", (kernel_ulong_t)&sc1501a_spec_sec },
	{ "sc1501a",    (kernel_ulong_t)&sc1501a_spec_pri },
	{}
};
MODULE_DEVICE_TABLE(i2c, sc1501a_i2c_id);

static struct i2c_driver sc1501a_driver = {
	.driver = {
		.name = "sc1501a",
		.of_match_table = of_match_ptr(sc1501a_of_match),
	},
	.probe    = sc1501a_probe,
	.remove   = sc1501a_remove,
	.id_table = sc1501a_i2c_id,
};

module_i2c_driver(sc1501a_driver);

MODULE_AUTHOR("Katsuhiro Suzuki <suzuki.katsuhiro@socionext.com>");
MODULE_DESCRIPTION("Socionext SC1501A series demodulator driver.");
MODULE_LICENSE("GPL v2");
