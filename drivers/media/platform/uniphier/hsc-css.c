// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
// CSS (Cross Stream Switch) connects MPEG2-TS input port and output port.
//
// Copyright (c) 2018 Socionext Inc.

//#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "hsc.h"
#include "hsc-reg.h"

int hsc_css_out_to_ts_in(int out)
{
	if (out >= HSC_CSS_OUT_TSI0 && out <= HSC_CSS_OUT_TSI9)
		return HSC_TS_IN0 + (out - HSC_CSS_OUT_TSI0);

	return -1;
}

int hsc_css_out_to_dpll_src(int out)
{
	if (out >= HSC_CSS_OUT_TSI0 && out <= HSC_CSS_OUT_TSI9)
		return HSC_DPLL_SRC_TSI0 + (out - HSC_CSS_OUT_TSI0);

	return -1;
}

static bool css_in_is_valid(struct hsc_chip *chip, int in)
{
	return in < chip->spec->num_css_in;
}

static bool css_out_is_valid(struct hsc_chip *chip, int out)
{
	return out < chip->spec->num_css_out;
}

static const struct hsc_spec_css *css_in_get_spec(struct hsc_chip *chip,
						  int in)
{
	const struct hsc_spec_css *spec = chip->spec->css_in;

	if (!css_in_is_valid(chip, in))
		return NULL;

	return &spec[in];
}

static const struct hsc_spec_css *css_out_get_spec(struct hsc_chip *chip,
						   int out)
{
	const struct hsc_spec_css *spec = chip->spec->css_out;

	if (!css_out_is_valid(chip, out))
		return NULL;

	return &spec[out];
}

int hsc_dpll_get_src(struct hsc_chip *chip, int dpll, int *src)
{
	struct regmap *r = chip->regmap;
	u32 v;

	if (!src || dpll >= HSC_DPLL_NUM)
		return -EINVAL;

	regmap_read(r, CSS_DPCTRL(dpll), &v);
	*src = ffs(v & CSS_DPCTRL_DPSEL_MASK) - 1;

	return 0;
}

/**
 * Select source clock of DPLL.
 *
 * @dpll: ID of DPLL, use one of HSC_DPLL_*
 * @src : ID of clock source, use one of HSC_DPLL_SRC_* or HSC_DPLL_SRC_NONE
 *        to disconnect
 */
int hsc_dpll_set_src(struct hsc_chip *chip, int dpll, int src)
{
	struct regmap *r = chip->regmap;
	u32 v = 0;

	if (dpll >= HSC_DPLL_NUM || src >= HSC_DPLL_SRC_NUM)
		return -EINVAL;

	if (src != HSC_DPLL_SRC_NONE)
		v = 1 << src;

	regmap_write(r, CSS_DPCTRL(dpll), v);

	return 0;
}

static int hsc_css_get_polarity(struct hsc_chip *chip,
				const struct hsc_reg_css_pol *pol,
				bool *sync_bit, bool *val_bit, bool *clk_fall)
{
	struct regmap *r = chip->regmap;
	u32 v;

	if (!sync_bit || !val_bit || !clk_fall || !pol->valid)
		return -EINVAL;

	regmap_read(r, pol->reg, &v);

	*sync_bit = !!(v & BIT(pol->sft_sync));
	*val_bit = !!(v & BIT(pol->sft_val));
	*clk_fall = !!(v & BIT(pol->sft_clk));

	return 0;
}

/**
 * Setup signal polarity of TS signals.
 *
 * @sync_bit : true : The sync signal keeps only 1bit period.
 *             false: The sync signal keeps during 8bits period.
 * @valid_bit: true : The valid signal does not keep during 8bits period.
 *             false: The valid signal keeps during 8bits period.
 * @clk_fall : true : Latch the data at falling edge of clock signal.
 *             false: Latch the data at rising edge of clock signal.
 */
static int hsc_css_set_polarity(struct hsc_chip *chip,
				const struct hsc_reg_css_pol *pol,
				bool sync_bit, bool val_bit, bool clk_fall)
{
	struct regmap *r = chip->regmap;
	u32 m = 0, v = 0;

	if (!pol->valid)
		return -EINVAL;

	if (pol->sft_sync != -1) {
		m |= BIT(pol->sft_sync);
		if (sync_bit)
			v |= BIT(pol->sft_sync);
	}

	if (pol->sft_val != -1) {
		m |= BIT(pol->sft_val);
		if (val_bit)
			v |= BIT(pol->sft_val);
	}

	if (pol->sft_clk != -1) {
		m |= BIT(pol->sft_clk);
		if (clk_fall)
			v |= BIT(pol->sft_clk);
	}

	regmap_update_bits(r, pol->reg, m, v);

	return 0;
}

int hsc_css_in_get_polarity(struct hsc_chip *chip, int in,
			    bool *sync_bit, bool *val_bit, bool *clk_fall)
{
	const struct hsc_spec_css *speci = css_in_get_spec(chip, in);

	if (!speci)
		return -EINVAL;

	return hsc_css_get_polarity(chip, &speci->pol,
				    sync_bit, val_bit, clk_fall);
}

int hsc_css_in_set_polarity(struct hsc_chip *chip, int in,
			    bool sync_bit, bool val_bit, bool clk_fall)
{
	const struct hsc_spec_css *speci = css_in_get_spec(chip, in);

	if (!speci)
		return -EINVAL;

	return hsc_css_set_polarity(chip, &speci->pol,
				    sync_bit, val_bit, clk_fall);
}

int hsc_css_out_get_polarity(struct hsc_chip *chip, int out,
			     bool *sync_bit, bool *val_bit, bool *clk_fall)
{
	const struct hsc_spec_css *speco = css_out_get_spec(chip, out);

	if (!speco)
		return -EINVAL;

	return hsc_css_get_polarity(chip, &speco->pol,
				    sync_bit, val_bit, clk_fall);
}

int hsc_css_out_set_polarity(struct hsc_chip *chip, int out,
			     bool sync_bit, bool val_bit, bool clk_fall)
{
	const struct hsc_spec_css *speco = css_out_get_spec(chip, out);

	if (!speco)
		return -EINVAL;

	return hsc_css_set_polarity(chip, &speco->pol,
				    sync_bit, val_bit, clk_fall);
}

int hsc_css_out_get_src(struct hsc_chip *chip, int *tsi, int out, bool *en)
{
	struct regmap *r = chip->regmap;
	const struct hsc_spec_css *speco = css_out_get_spec(chip, out);
	u32 v;

	if (!tsi || !en || !speco || !speco->sel.valid)
		return -EINVAL;

	regmap_read(r, speco->sel.reg, &v);
	*tsi = (v & speco->sel.mask) >> speco->sel.sft;

	regmap_read(r, CSS_OUTPUTENABLE, &v);
	*en = !!(v & BIT(out));

	return 0;
}

/**
 * Connect the input port and output port using CSS (Cross Stream Switch).
 *
 * @in   : Input port number, use one of HSC_CSS_IN_*.
 * @out  : Output port number, use one of HSC_CSS_OUT_*.
 * @en   : false: Disable this path.
 *         true : Enable this path.
 */
int hsc_css_out_set_src(struct hsc_chip *chip, int in, int out, bool en)
{
	struct regmap *r = chip->regmap;
	const struct hsc_spec_css *speco = css_out_get_spec(chip, out);

	if (!css_in_is_valid(chip, in) || !speco || !speco->sel.valid)
		return -EINVAL;

	regmap_update_bits(r, speco->sel.reg, speco->sel.mask,
			   in << speco->sel.sft);

	regmap_update_bits(r, CSS_OUTPUTENABLE, BIT(out), (en) ? ~0 : 0);

	return 0;
}
