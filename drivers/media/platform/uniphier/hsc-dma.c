// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
// MPEG2-TS DMA control.
//
// Copyright (c) 2018 Socionext Inc.

//#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "hsc.h"
#include "hsc-reg.h"

u64 hsc_rb_cnt(struct hsc_dma_buf *buf)
{
	if (buf->rd_offs <= buf->wr_offs)
		return buf->wr_offs - buf->rd_offs;
	else
		return buf->size - (buf->rd_offs - buf->wr_offs);
}

u64 hsc_rb_cnt_to_end(struct hsc_dma_buf *buf)
{
	if (buf->rd_offs <= buf->wr_offs)
		return buf->wr_offs - buf->rd_offs;
	else
		return buf->size - buf->rd_offs;
}

u64 hsc_rb_space(struct hsc_dma_buf *buf)
{
	if (buf->rd_offs <= buf->wr_offs)
		return buf->size - (buf->wr_offs - buf->rd_offs) - 8;
	else
		return buf->rd_offs - buf->wr_offs - 8;
}

u64 hsc_rb_space_to_end(struct hsc_dma_buf *buf)
{
	if (buf->rd_offs > buf->wr_offs)
		return buf->rd_offs - buf->wr_offs - 8;
	else if (buf->rd_offs > 0)
		return buf->size - buf->wr_offs;
	else
		return buf->size - buf->wr_offs - 8;
}

void hsc_dma_rb_set_buffer(struct hsc_chip *chip, int rb_ch, u64 bg, u64 ed)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, CDMBC_RBBGNADRSD(rb_ch), bg);
	regmap_write(r, CDMBC_RBBGNADRSU(rb_ch), bg >> 32);
	regmap_write(r, CDMBC_RBENDADRSD(rb_ch), ed);
	regmap_write(r, CDMBC_RBENDADRSU(rb_ch), ed >> 32);
}

u64 hsc_dma_rb_get_rp(struct hsc_chip *chip, int rb_ch)
{
	struct regmap *r = chip->regmap;
	u32 d, u;

	regmap_read(r, CDMBC_RBRDPTRD(rb_ch), &d);
	regmap_read(r, CDMBC_RBRDPTRU(rb_ch), &u);

	return ((u64)u << 32) | d;
}

void hsc_dma_rb_set_rp(struct hsc_chip *chip, int rb_ch, u64 pos)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, CDMBC_RBRDPTRD(rb_ch), pos);
	regmap_write(r, CDMBC_RBRDPTRU(rb_ch), pos >> 32);
}

u64 hsc_dma_rb_get_wp(struct hsc_chip *chip, int rb_ch)
{
	struct regmap *r = chip->regmap;
	u32 d, u;

	regmap_read(r, CDMBC_RBWRPTRD(rb_ch), &d);
	regmap_read(r, CDMBC_RBWRPTRU(rb_ch), &u);

	return ((u64)u << 32) | d;
}

void hsc_dma_rb_set_wp(struct hsc_chip *chip, int rb_ch, u64 pos)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, CDMBC_RBWRPTRD(rb_ch), pos);
	regmap_write(r, CDMBC_RBWRPTRU(rb_ch), pos >> 32);
}

static void dma_set_chkp(struct hsc_chip *chip, int dma_ch, u64 pos)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, CDMBC_CHIRADRSD(dma_ch), pos);
	regmap_write(r, CDMBC_CHIRADRSU(dma_ch), pos >> 32);
}

static void dma_set_enable(struct hsc_chip *chip, int dma_ch,
			   const struct hsc_reg_cmn *dma_en, bool en)
{
	struct regmap *r = chip->regmap;
	u32 v;
	bool now;

	regmap_read(r, dma_en->reg, &v);
	now = !!(v & BIT(dma_en->sft));

	/* Toggle DMA state if needed */
	if ((en && !now) || (!en && now))
		regmap_write(r, dma_en->reg, BIT(dma_en->sft));
}

static bool dma_out_is_valid(struct hsc_chip *chip, int out)
{
	return out < chip->spec->num_dma_out ||
		chip->spec->dma_out[out].intr.valid;
}

int hsc_dma_out_init(struct hsc_dma *dma_out, struct hsc_chip *chip,
		     int id, struct hsc_dma_buf *buf)
{
	if (!dma_out || !dma_out_is_valid(chip, id))
		return -EINVAL;

	dma_out->chip = chip;
	dma_out->id = id;
	dma_out->spec = &chip->spec->dma_out[id];
	dma_out->buf = buf;

	return 0;
}

void hsc_dma_out_set_src_ts_in(struct hsc_dma *dma_out, int tsi)
{
	struct regmap *r = dma_out->chip->regmap;
	const struct hsc_spec_dma *spec = dma_out->spec;
	u32 m, v;

	m = CDMBC_CHTDCTRLH_STREM_MASK | CDMBC_CHTDCTRLH_ALL_EN;
	v = FIELD_PREP(CDMBC_CHTDCTRLH_STREM_MASK, tsi) |
		CDMBC_CHTDCTRLH_ALL_EN;
	regmap_update_bits(r, CDMBC_CHTDCTRLH(spec->td_ch), m, v);
}

void hsc_dma_out_start(struct hsc_dma *dma_out, bool en)
{
	struct hsc_chip *chip = dma_out->chip;
	const struct hsc_spec_dma *spec = dma_out->spec;
	struct hsc_dma_buf *buf = dma_out->buf;
	struct regmap *r = chip->regmap;
	u64 bg, ed;
	u32 v;

	bg = buf->phys;
	ed = buf->phys + buf->size;
	hsc_dma_rb_set_buffer(chip, spec->rb_ch, bg, ed);

	buf->rd_offs = 0;
	buf->wr_offs = 0;
	buf->chk_offs = buf->size_chk;
	hsc_dma_rb_set_rp(chip, spec->rb_ch, buf->rd_offs + buf->phys);
	hsc_dma_rb_set_wp(chip, spec->rb_ch, buf->wr_offs + buf->phys);
	dma_set_chkp(chip, spec->dma_ch, buf->chk_offs + buf->phys);

	regmap_update_bits(r, CDMBC_CHDSTAMODE(spec->dma_ch),
			   CDMBC_CHAMODE_TYPE_RB, ~0);
	regmap_update_bits(r, CDMBC_CHCTRL1(spec->dma_ch),
			   CDMBC_CHCTRL1_IND_SIZE_UND, ~0);

	v = (en) ? ~0 : 0;
	regmap_update_bits(r, CDMBC_CHIE(spec->dma_ch), CDMBC_CHI_TRANSIT, v);
	regmap_update_bits(r, spec->intr.reg, BIT(spec->intr.sft), v);

	dma_set_enable(chip, spec->dma_ch, &spec->en, en);
}

void hsc_dma_out_sync(struct hsc_dma *dma_out)
{
	struct hsc_chip *chip = dma_out->chip;
	const struct hsc_spec_dma *spec = dma_out->spec;
	struct hsc_dma_buf *buf = dma_out->buf;

	hsc_dma_rb_set_rp(chip, spec->rb_ch, buf->rd_offs + buf->phys);
	buf->wr_offs = hsc_dma_rb_get_wp(chip, spec->rb_ch) - buf->phys;
	dma_set_chkp(chip, spec->dma_ch, buf->chk_offs + buf->phys);
}

int hsc_dma_out_get_intr(struct hsc_dma *dma_out, u32 *stat)
{
	struct regmap *r = dma_out->chip->regmap;

	if (!stat)
		return -EINVAL;

	regmap_read(r, CDMBC_CHID(dma_out->spec->dma_ch), stat);

	return 0;
}

void hsc_dma_out_clear_intr(struct hsc_dma *dma_out, u32 clear)
{
	struct regmap *r = dma_out->chip->regmap;

	regmap_write(r, CDMBC_CHIR(dma_out->spec->dma_ch), clear);
}
