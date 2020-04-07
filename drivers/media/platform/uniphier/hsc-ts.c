// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
// MPEG2-TS input/output port setting.
//
// Copyright (c) 2018 Socionext Inc.

//#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "hsc.h"
#include "hsc-reg.h"

#define PARAMA_OFFSET_TS      0x02
#define PARAMA_LOOPADDR_TS    0x31
#define PARAMA_COUNT_TS       0xc4

static bool ts_in_is_valid(struct hsc_chip *chip, int tsi)
{
	return tsi < chip->spec->num_ts_in && chip->spec->ts_in[tsi].intr.valid;
}

static const struct hsc_spec_ts *ts_in_get_spec(struct hsc_chip *chip, int tsi)
{
	const struct hsc_spec_ts *spec = chip->spec->ts_in;

	if (!ts_in_is_valid(chip, tsi))
		return NULL;

	return &spec[tsi];
}

int hsc_ts_in_set_enable(struct hsc_chip *chip, int tsi, bool en)
{
	struct regmap *r = chip->regmap;
	const struct hsc_spec_ts *speci = ts_in_get_spec(chip, tsi);
	u32 m, v;

	if (!speci)
		return -EINVAL;

	m = TSI_SYNCCNTROL_FRAME_MASK;
	v = TSI_SYNCCNTROL_FRAME_EXTSYNC2;
	regmap_update_bits(r, TSI_SYNCCNTROL(tsi), m, v);

	m = TSI_CONFIG_ATSMD_MASK | TSI_CONFIG_STCMD_MASK |
		TSI_CONFIG_CHEN_START;
	v = TSI_CONFIG_ATSMD_DPLL | TSI_CONFIG_STCMD_DPLL;
	if (en)
		v |= TSI_CONFIG_CHEN_START;
	regmap_update_bits(r, TSI_CONFIG(tsi), m, v);

	v = (en) ? ~0 : 0;
	regmap_update_bits(r, TSI_INTREN(tsi),
			   TSI_INTR_SERR | TSI_INTR_LOST, v);
	regmap_update_bits(r, speci->intr.reg, BIT(speci->intr.sft), v);

	return 0;
}

int hsc_ts_in_set_dmaparam(struct hsc_chip *chip, int tsi, int ifmt)
{
	struct regmap *r = chip->regmap;
	u32 v, ats, offset, loop, cnt;

	if (!ts_in_is_valid(chip, tsi))
		return -EINVAL;

	switch (ifmt) {
	case HSC_TSIF_MPEG2_TS:
		ats = 0;
		offset = PARAMA_OFFSET_TS;
		loop = PARAMA_LOOPADDR_TS;
		cnt = PARAMA_COUNT_TS;
		break;
	case HSC_TSIF_MPEG2_TS_ATS:
		ats = TSI_CONFIG_ATSADD_ON;
		offset = PARAMA_OFFSET_TS;
		loop = PARAMA_LOOPADDR_TS;
		cnt = PARAMA_COUNT_TS;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(r, TSI_CONFIG(tsi), TSI_CONFIG_ATSADD_ON, ats);

	v = FIELD_PREP(SBC_DMAPARAMA_OFFSET_MASK, offset) |
		FIELD_PREP(SBC_DMAPARAMA_LOOPADDR_MASK, loop) |
		FIELD_PREP(SBC_DMAPARAMA_COUNT_MASK, cnt);
	regmap_write(r, SBC_DMAPARAMA(tsi), v);

	return 0;
}

int hsc_ts_in_get_intr(struct hsc_chip *chip, int tsi, u32 *stat)
{
	struct regmap *r = chip->regmap;

	if (!stat)
		return -EINVAL;

	regmap_read(r, TSI_SYNCSTATUS(tsi), stat);

	return 0;
}

void hsc_ts_in_clear_intr(struct hsc_chip *chip, int tsi, u32 clear)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, TSI_SYNCSTATUS(tsi), clear);
}
