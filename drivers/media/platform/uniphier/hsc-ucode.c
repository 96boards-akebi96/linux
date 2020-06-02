// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
// Core init and uCode loader.
//
// Copyright (c) 2018 Socionext Inc.

//#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "hsc.h"
#include "hsc-reg.h"

struct hsc_cip_file_dma_param {
	dma_addr_t cipr_start;
	dma_addr_t cipw_start;
	size_t inter_size;
	size_t total_size;
	u8 key_id1;
	u8 key_id0;
	u8 endian;
	int id1_en;
	int push;
};

static void core_start(struct hsc_chip *chip)
{
	const struct hsc_spec_init_ram *rams = chip->spec->init_rams;
	struct regmap *r = chip->regmap;
	size_t i, s;

	regmap_write(r, IOB_RESET0, ~0);
	regmap_write(r, IOB_RESET1, ~0);

	regmap_write(r, IOB_CLKSTOP, 0);
	/* Deassert all internal resets, but AP core is later for uCode */
	regmap_write(r, IOB_RESET0, IOB_RESET0_APCORE);
	regmap_write(r, IOB_RESET1, 0);

	/* Halt SPU for uCode */
	regmap_write(r, IOB_DEBUG, IOB_DEBUG_SPUHALT);

	for (i = 0; i < chip->spec->num_init_rams; i++)
		for (s = 0; s < rams[i].size; s += 4)
			regmap_write(r, rams[i].addr + s, rams[i].pattern);
}

static void core_stop(struct hsc_chip *chip)
{
	struct regmap *r = chip->regmap;

	regmap_write(r, IOB_RESET0, 0);
	regmap_write(r, IOB_RESET1, 0);

	regmap_write(r, IOB_CLKSTOP, ~0);
}

static int ucode_set_data_addr(struct hsc_chip *chip, int mode)
{
	struct regmap *r = chip->regmap;
	dma_addr_t addr;

	switch (mode) {
	case HSC_UCODE_SPU_0:
	case HSC_UCODE_SPU_1:
		addr = chip->ucode_spu.phys_data;
		regmap_write(r, UCODE_DLADDR0, addr);
		regmap_write(r, UCODE_DLADDR1, addr >> 32);
		break;
	case HSC_UCODE_ACE:
		addr = chip->ucode_am.phys_data;
		regmap_write(r, CIP_UCODEADDR_AM0, addr);
		regmap_write(r, CIP_UCODEADDR_AM1, addr >> 32);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void file_channel_dma_set(struct hsc_chip *chip,
				 const struct hsc_spec_dma *spec_r,
				 const struct hsc_spec_dma *spec_w,
				 struct hsc_cip_file_dma_param *p)
{
	struct regmap *r = chip->regmap;
	dma_addr_t cipr_end, cipw_end;
	u32 v;

	/* For CIP Read */
	v = FIELD_PREP(CDMBC_CHCTRL1_LINKCH1_MASK, 1) |
		FIELD_PREP(CDMBC_CHCTRL1_STATSEL_MASK, 4) |
		CDMBC_CHCTRL1_TYPE_INTERMIT;
	regmap_write(r, CDMBC_CHCTRL1(spec_r->dma_ch), v);

	regmap_write(r, CDMBC_CHCAUSECTRL(spec_r->dma_ch), 0);

	v = FIELD_PREP(CDMBC_CHAMODE_ENDIAN_MASK, p->endian) |
		FIELD_PREP(CDMBC_CHAMODE_AUPDT_MASK, 0) |
		CDMBC_CHAMODE_TYPE_RB;
	regmap_write(r, CDMBC_CHSRCAMODE(spec_r->dma_ch), v);

	v = FIELD_PREP(CDMBC_CHAMODE_ENDIAN_MASK, 1) |
		FIELD_PREP(CDMBC_CHAMODE_AUPDT_MASK, 2);
	regmap_write(r, CDMBC_CHDSTAMODE(spec_r->dma_ch), v);

	v = FIELD_PREP(CDMBC_CHDSTSTRTADRS_TID_MASK, 0xc) |
		FIELD_PREP(CDMBC_CHDSTSTRTADRS_ID1_EN_MASK, p->id1_en) |
		FIELD_PREP(CDMBC_CHDSTSTRTADRS_KEY_ID1_MASK, p->key_id1) |
		FIELD_PREP(CDMBC_CHDSTSTRTADRS_KEY_ID0_MASK, p->key_id0);
	regmap_write(r, CDMBC_CHDSTSTRTADRSD(spec_r->dma_ch), v);

	regmap_write(r, CDMBC_CHSIZE(spec_r->dma_ch), p->inter_size);

	cipr_end = p->cipr_start + p->total_size;
	hsc_dma_rb_set_buffer(chip, spec_r->rb_ch, p->cipr_start, cipr_end);
	hsc_dma_rb_set_rp(chip, spec_r->rb_ch, p->cipr_start);
	hsc_dma_rb_set_wp(chip, spec_r->rb_ch, cipr_end);

	/* For CIP Write */
	v = FIELD_PREP(CDMBC_CHCTRL1_LINKCH1_MASK, 5) |
		FIELD_PREP(CDMBC_CHCTRL1_STATSEL_MASK, 4) |
		CDMBC_CHCTRL1_TYPE_INTERMIT |
		CDMBC_CHCTRL1_IND_SIZE_UND;
	regmap_write(r, CDMBC_CHCTRL1(spec_w->dma_ch), v);

	v = FIELD_PREP(CDMBC_CHAMODE_ENDIAN_MASK, 1) |
		FIELD_PREP(CDMBC_CHAMODE_AUPDT_MASK, 2);
	regmap_write(r, CDMBC_CHSRCAMODE(spec_w->dma_ch), v);

	v = FIELD_PREP(CDMBC_CHAMODE_ENDIAN_MASK, p->endian) |
		FIELD_PREP(CDMBC_CHAMODE_AUPDT_MASK, 0) |
		CDMBC_CHAMODE_TYPE_RB;
	regmap_write(r, CDMBC_CHDSTAMODE(spec_w->dma_ch), v);

	cipw_end = p->cipw_start + p->total_size;
	hsc_dma_rb_set_buffer(chip, spec_w->rb_ch, p->cipw_start, cipw_end);
	hsc_dma_rb_set_rp(chip, spec_w->rb_ch, cipw_end);
	hsc_dma_rb_set_wp(chip, spec_w->rb_ch, p->cipw_start);

	/* Transferring size */
	regmap_write(r, CDMBC_ITSTEPS(spec_r->it_ch), p->total_size);

	/* CIP settings */
	regmap_write(r, CDMBC_CIPMODE(spec_r->cip_ch),
		     (p->push) ? CDMBC_CIPMODE_PUSH : 0);

	regmap_write(r, CDMBC_CIPPRIORITY(spec_r->cip_ch),
		     FIELD_PREP(CDMBC_CIPPRIORITY_PRIOR_MASK, 3));
}

static void file_channel_start(struct hsc_chip *chip,
			       const struct hsc_spec_dma *spec_r,
			       const struct hsc_spec_dma *spec_w,
			       bool push, bool mmu_en)
{
	struct regmap *r = chip->regmap;
	u32 v;

	regmap_write(r, CDMBC_CIPMODE(spec_r->cip_ch),
		     (push) ? CDMBC_CIPMODE_PUSH : 0);

	if (mmu_en) {
		v = CDMBC_CHDDR_REG_LOAD_ON | CDMBC_CHDDR_AT_CHEN_ON;

		/* Enable IOMMU for CIP-R and CIP-W */
		regmap_write(r, CDMBC_CHDDR(spec_r->dma_ch),
			     v | CDMBC_CHDDR_SET_MCB_RD);
		regmap_write(r, CDMBC_CHDDR(spec_w->dma_ch),
			     v | CDMBC_CHDDR_SET_MCB_WR);
	}

	v = 0x01000000 | (1 << spec_r->en.sft) | (1 << spec_w->en.sft);
	regmap_write(r, CDMBC_STRT(1), v);
}

static void file_channel_wait(struct hsc_chip *chip,
			      const struct hsc_spec_dma *spec)
{
	struct regmap *r = chip->regmap;
	u32 v;

	regmap_read(r, CDMBC_CHIR(spec->dma_ch), &v);
	while (!(v & INTR_MBC_CH_WDONE)) {
		usleep_range(1000, 10000);
		regmap_read(r, CDMBC_CHIR(spec->dma_ch), &v);
	};
	regmap_write(r, CDMBC_CHIR(spec->dma_ch), v);

	regmap_read(r, CDMBC_RBIR(spec->dma_ch), &v);
	regmap_write(r, CDMBC_RBIR(spec->dma_ch), v);
}

static int ucode_load_dma(struct hsc_chip *chip, int mode)
{
	const struct hsc_spec_dma *spec_r, *spec_w;
	struct regmap *r = chip->regmap;
	struct hsc_ucode_buf *ucode;
	struct hsc_cip_file_dma_param dma_p = {0};
	u32 cip_f_ctrl;

	spec_r = &chip->spec->dma_in[HSC_DMA_CIP_IN0];
	spec_w = &chip->spec->dma_out[HSC_DMA_CIP_OUT0];

	switch (mode) {
	case HSC_UCODE_SPU_0:
	case HSC_UCODE_SPU_1:
		ucode = &chip->ucode_spu;
		cip_f_ctrl = 0x2f090001;
		break;
	case HSC_UCODE_ACE:
		ucode = &chip->ucode_am;
		cip_f_ctrl = 0x3f090001;
		break;
	default:
		return -EINVAL;
	}

	regmap_write(r, CIP_F_CTRL, cip_f_ctrl);

	dma_p.cipr_start = ucode->phys_code;
	dma_p.cipw_start = 0;
	dma_p.inter_size = ucode->size_code;
	dma_p.total_size = ucode->size_code;
	dma_p.key_id1 = 0;
	dma_p.key_id0 = 0;
	dma_p.endian = 1;
	dma_p.id1_en = 0;
	file_channel_dma_set(chip, spec_r, spec_w, &dma_p);
	file_channel_start(chip, spec_r, spec_w, true, false);

	file_channel_wait(chip, spec_r);
	file_channel_wait(chip, spec_w);

	return 0;
}

static int ucode_load(struct hsc_chip *chip, int mode)
{
	struct device *dev = &chip->pdev->dev;
	const struct hsc_spec_ucode *spec;
	struct hsc_ucode_buf *ucode;
	const struct firmware *firm_code, *firm_data;
	int ret;

	switch (mode) {
	case HSC_UCODE_SPU_0:
	case HSC_UCODE_SPU_1:
		spec = &chip->spec->ucode_spu;
		ucode = &chip->ucode_spu;
		break;
	case HSC_UCODE_ACE:
		spec = &chip->spec->ucode_ace;
		ucode = &chip->ucode_am;
		break;
	default:
		return -EINVAL;
	}

	ret = request_firmware(&firm_code, spec->name_code, dev);
	if (ret) {
		dev_err(dev, "Failed to load firmware '%s'.\n",
			spec->name_code);
		return ret;
	}

	ret = request_firmware(&firm_data, spec->name_data, dev);
	if (ret) {
		dev_err(dev, "Failed to load firmware '%s'.\n",
			spec->name_data);
		goto err_firm_code;
	}

	ucode->buf_code = dma_alloc_coherent(dev, firm_code->size,
					     &ucode->phys_code, GFP_KERNEL);
	if (!ucode->buf_code) {
		ret = -ENOMEM;
		goto err_firm_data;
	}
	ucode->size_code = firm_code->size;

	ucode->buf_data = dma_alloc_coherent(dev, firm_data->size,
					     &ucode->phys_data, GFP_KERNEL);
	if (!ucode->buf_data) {
		ret = -ENOMEM;
		goto err_buf_code;
	}
	ucode->size_data = firm_data->size;

	memcpy(ucode->buf_code, firm_code->data, firm_code->size);
	memcpy(ucode->buf_data, firm_data->data, firm_data->size);

	ret = ucode_set_data_addr(chip, mode);
	if (ret)
		goto err_buf_data;

	ret = ucode_load_dma(chip, mode);
	if (ret)
		goto err_buf_data;

	release_firmware(firm_data);
	release_firmware(firm_code);

	return 0;

err_buf_data:
	dma_free_coherent(dev, ucode->size_data, ucode->buf_data,
			  ucode->phys_data);

err_buf_code:
	dma_free_coherent(dev, ucode->size_code, ucode->buf_code,
			  ucode->phys_code);

err_firm_data:
	release_firmware(firm_data);

err_firm_code:
	release_firmware(firm_code);

	return ret;
}

static int ucode_unload(struct hsc_chip *chip, int mode)
{
	struct device *dev = &chip->pdev->dev;
	struct hsc_ucode_buf *ucode;

	switch (mode) {
	case HSC_UCODE_SPU_0:
	case HSC_UCODE_SPU_1:
		ucode = &chip->ucode_spu;
		break;
	case HSC_UCODE_ACE:
		ucode = &chip->ucode_am;
		break;
	default:
		return -EINVAL;
	}

	dma_free_coherent(dev, ucode->size_data, ucode->buf_data,
			  ucode->phys_data);
	dma_free_coherent(dev, ucode->size_code, ucode->buf_code,
			  ucode->phys_code);

	return 0;
}

int hsc_ucode_load_all(struct hsc_chip *chip)
{
	struct regmap *r = chip->regmap;
	int ret;

	core_start(chip);

	ret = ucode_load(chip, HSC_UCODE_SPU_0);
	if (ret)
		return ret;

	/* Start SPU core */
	regmap_write(r, IOB_DEBUG, 0);

	ret = ucode_load(chip, HSC_UCODE_ACE);
	if (ret)
		return ret;

	/* Start AP core */
	regmap_write(r, IOB_RESET0, 0);

	return 0;
}

int hsc_ucode_unload_all(struct hsc_chip *chip)
{
	int ret;

	core_stop(chip);

	ret = ucode_unload(chip, HSC_UCODE_SPU_0);
	if (ret)
		return ret;

	ret = ucode_unload(chip, HSC_UCODE_ACE);
	if (ret)
		return ret;

	return 0;
}
