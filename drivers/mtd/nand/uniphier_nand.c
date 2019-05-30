/* $Id$
 * uniphier_nand.c: MTD NAND flash driver for uniphier nand controller
 *
 * Copyright (C) 2016 Socionext Corporation
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include "uniphier_nand.h"
#include "uniphier_nand_reg.h"

#if !((0 < NAND_GPBC_FLASH_BANKS) && (NAND_GPBC_FLASH_BANKS <= NAND_GPBC_MAX_FLASH_BANKS))
#error "Error: Invalid NAND_GPBC_FLASH_BANKS."
#endif

static uint8_t nand_gpbc_bbt_pattern[] = NAND_GPBC_UNIT_BBT_PATTERN_MASTER;
static struct nand_bbt_descr nand_gpbc_BbtMasterDesc = {
	.options	= NAND_GPBC_BBT_OPTION,
	.offs		= NAND_GPBC_BBT_OFFS,
	.len		= NAND_GPBC_BBT_LEN,
	.veroffs	= NAND_GPBC_BBT_VEROFFS,
	.maxblocks	= NAND_GPBC_UNIT_BBT_MAXBLOCKS,
	.pattern	= nand_gpbc_bbt_pattern
};
static uint8_t nand_gpbc_mirror_pattern[] = NAND_GPBC_UNIT_BBT_PATTERN_MIRROR;
static struct nand_bbt_descr nand_gpbc_BbtMirrorDesc = {
	.options	= NAND_GPBC_BBT_OPTION,
	.offs		= NAND_GPBC_BBT_OFFS,
	.len		= NAND_GPBC_BBT_LEN,
	.veroffs	= NAND_GPBC_BBT_VEROFFS,
	.maxblocks	= NAND_GPBC_UNIT_BBT_MAXBLOCKS,
	.pattern	= nand_gpbc_mirror_pattern
};

static struct nand_ecclayout nand_gpbc_EccLayout = {
	.eccbytes	= 0,
	.eccpos		= NAND_GPBC_UNIT_OOB_ECCPOS,
};

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
static uint8_t nand_gpbc_bbt_flag_ff_pattern[] = NAND_GPBC_BBT_FLAG_FF_PATTERN;
static struct nand_bbt_descr nand_gpbc_BbtFlagDesc = {
	.options	= (NAND_GPBC_BBT_OPTION & (~NAND_BBT_NO_OOB)),
	.offs		= NAND_GPBC_BBT_FLAG_OFFS,
	.len		= NAND_GPBC_BBT_FLAG_LEN,
	.pattern	= nand_gpbc_bbt_flag_ff_pattern
};
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

static uint8_t nand_gpbc_scan_ff_pattern[] = NAND_GPBC_BBTSCAN_FF_PATTERN;
static struct nand_bbt_descr nand_gpbc_BbPatternDesc = {
	.options	= (NAND_GPBC_BBT_OPTION & (~NAND_BBT_NO_OOB)),
	.offs		= NAND_GPBC_BBTSCAN_OFFS,
	.len		= NAND_GPBC_BBTSCAN_LEN,
	.pattern	= nand_gpbc_scan_ff_pattern
};

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
static struct nand_bbm	nand_gpbc_ABbm[NAND_MAX_CHIPS];
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

static const struct nand_soc_flags_table soc_flags_table[] = {
	{ "socionext,ph1-sld3",
	  { .ecc_sw_ctrl = 0, .set_cdma_erren = 0, .set_nces_twb = 0,
	    .set_ndb     = 0, .ddma_comp_fix  = 0
	  }
	},
	{ "socionext,ph1-ld4",
	  { .ecc_sw_ctrl = 0, .set_cdma_erren = 0, .set_nces_twb = 0,
	    .set_ndb     = 0, .ddma_comp_fix  = 0
	  }
	},
	{ "socionext,ph1-pro4",
	  { .ecc_sw_ctrl = 1, .set_cdma_erren = 0, .set_nces_twb = 0,
	    .set_ndb     = 0, .ddma_comp_fix  = 0
	  }
	},
	{ "socionext,ph1-sld8",
	  { .ecc_sw_ctrl = 1, .set_cdma_erren = 0, .set_nces_twb = 0,
	    .set_ndb     = 0, .ddma_comp_fix  = 1
	  }
	},
	{ "other",
	  { .ecc_sw_ctrl = 2, .set_cdma_erren = 1, .set_nces_twb = 1,
	    .set_ndb     = 1, .ddma_comp_fix  = 1
	  }
	},
};

/* this macro allows us to convert from an MTD structure to our own
 * device context (gpbc) structure.
 */
#define mtd_to_gpbc(m) container_of(m, struct nand_gpbc_info, mtd)

extern void nand_proc_exit(void);
extern int nand_proc_init(struct nand_gpbc_info *gpbc);
extern struct nand_gpbc_timing *nand_gpbc_get_timing(struct nand_gpbc_info *gpbc);

static void nand_gpbc_get_soc_restrict(struct nand_gpbc_info *gpbc)
{
	int i, loop;

	loop = sizeof(soc_flags_table) / sizeof(struct nand_soc_flags_table) - 1;
	for(i = 0; i < loop; i++) {
		if(of_machine_is_compatible(soc_flags_table[i].name)) {
			gpbc->soc_flags = soc_flags_table[i].soc_flags;
			break;
		}
	}
	if(i == loop) {
		gpbc->soc_flags = soc_flags_table[i].soc_flags;
	}

	pr_info("[uniphier-nand] soc_flags: %d %d %d %d %d\n",
			 gpbc->soc_flags.ecc_sw_ctrl,
			 gpbc->soc_flags.set_cdma_erren,
			 gpbc->soc_flags.set_nces_twb,
			 gpbc->soc_flags.set_ndb,
			 gpbc->soc_flags.ddma_comp_fix);
}

//----------------------------
//------ layer 1 func --------
//----------------------------
static void nand_gpbc_index32(struct nand_gpbc_info *gpbc, u32 cmd, u32 data)
{
	iowrite32(cmd, (void __iomem *)gpbc->memBase);
	iowrite32(data, (void __iomem *)(gpbc->memBase + 0x10));
}

static u32 nand_gpbc_index32_read_data(struct nand_gpbc_info *gpbc, u32 cmd)
{
	iowrite32(cmd, (void __iomem *)gpbc->memBase);
	return ioread32((void __iomem *)(gpbc->memBase + 0x10));
}

static u32 nand_gpbc_read32(void *addr)
{
	return ioread32((void __iomem *)addr);
}

static void nand_gpbc_write32(u32 data, void *addr)
{
	iowrite32(data, (void __iomem *)addr);
	/* dummy read for bus coherency between reg port and data port */
	(void)ioread32((void __iomem *)addr);
}

static void nand_gpbc_set_eccen(struct nand_gpbc_info *gpbc, u32 data)
{
	if(gpbc->soc_flags.ecc_sw_ctrl)
		nand_gpbc_write32(data, gpbc->regBase + NAND_GPBC_NECCE);
}

static void nand_gpbc_clearIntStatus(struct nand_gpbc_info *gpbc, int bank)
{
	nand_gpbc_write32(NAND_GPBC_NI__MASK, gpbc->regBase + gpbc->nist[bank]);
}

#ifdef CONFIG_MTD_NAND_UNIPHIER_IRQ
static irqreturn_t nand_gpbc_irq(int this_irq, void *dev_id)
{
	struct nand_gpbc_info *gpbc = (struct nand_gpbc_info *)dev_id;

	nand_gpbc_write32(0, gpbc->regBase + gpbc->nie[0]);
	nand_gpbc_write32(0, gpbc->regBase + gpbc->nie[1]);
	complete(&gpbc->complete);

	return IRQ_HANDLED;
}
#endif

static u32 nand_gpbc_waitIrq(struct nand_gpbc_info *gpbc, int bank, u32 mask)
{
#ifdef CONFIG_MTD_NAND_UNIPHIER_IRQ
	unsigned long status;
	u32 result;

	nand_gpbc_write32(mask, gpbc->regBase + gpbc->nie[bank]);

	status = wait_for_completion_timeout(&gpbc->complete, msecs_to_jiffies(NAND_GPBC_IRQ_TIMEOUT));

	nand_gpbc_write32(0, gpbc->regBase + gpbc->nie[bank]);
	result = nand_gpbc_read32(gpbc->regBase + gpbc->nist[bank]);
	if (!status) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		panic( "%s:%d\n", __FILE__, __LINE__ );
	}
#else
	u32		result;
	unsigned long	timeo;

	timeo = jiffies + ((u64)NAND_GPBC_CMD_TIMEOUT * CONFIG_HZ + 999) / 1000;
	do {
		result = nand_gpbc_read32(gpbc->regBase + gpbc->nist[bank]);
		if (result & mask) {
			return result;
		}
		cond_resched();
	} while (time_before(jiffies, timeo));

	if (!(result & mask)) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}
#endif
	return result;
}

#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
static void nand_gpbc_set_cmd_desc_read_page(struct nand_gpbc_cmd_desc_t *cmd_desc,
					     u32 flash_page, u16 cmd_flags, u64 mem_ptr)
{
	cmd_desc->next_ptr = 0;
	cmd_desc->flash_page = flash_page;
	cmd_desc->reserved0 = 0;
	cmd_desc->cmd_type = 0x2001;	//read-ahead
	cmd_desc->cmd_flags = cmd_flags;
	cmd_desc->sync_arg = 0;
	cmd_desc->mem_ptr = mem_ptr;
	cmd_desc->status = 0;
	cmd_desc->reserved1 = 0;
	cmd_desc->reserved2 = 0;
	cmd_desc->sync_flag_ptr = 0;
	cmd_desc->mem_copy_addr = 0;
	cmd_desc->meta_data_addr = 0;
}

static u32 nand_gpbc_poll_cmd_dma(struct nand_gpbc_info *gpbc)
{
	volatile struct nand_gpbc_cmd_desc_t *cmd_desc = gpbc->cmd_desc;
	while (1) {
		if (cmd_desc->status & NAND_GPBC_CDESC_STAT_CMP) {
			break;
		}
	}
	return (u32)cmd_desc->status;
}
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

#ifndef CONFIG_MTD_NAND_UNIPHIER_DMA
static u32 nand_gpbc_checkIrq(struct nand_gpbc_info *gpbc, int bank)
{
	u32		result;

	result = nand_gpbc_read32(gpbc->regBase + gpbc->nist[bank]);

	return result;
}
#endif

static int nand_gpbc_get_max_bitflips(struct nand_gpbc_info *gpbc, int bank)
{
	int max_bitflips;

#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	max_bitflips = (gpbc->cmd_desc->status & NAND_GPBC_CDESC_STAT_MAX_ERR) >>
		NAND_GPBC_CDESC_STAT_MAX_ERR_SHIFT;
#else /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
	max_bitflips = (nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NECCINF) >> (8 * bank)) & 0x7f;
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

	return max_bitflips;
}


//----------------------------
//------ layer 2 func --------
//----------------------------
static void nand_gpbc_reset_buf(struct nand_gpbc_info *gpbc)
{
	gpbc->buf.head = gpbc->buf.tail = 0;
}

static void nand_gpbc_write_byte_to_buf(struct nand_gpbc_info *gpbc, uint8_t byte)
{
	BUG_ON(gpbc->buf.tail >= sizeof(gpbc->buf.buf));
	gpbc->buf.buf[gpbc->buf.tail++] = byte;
}

static void nand_gpbc_read_status(struct nand_gpbc_info *gpbc)
{
	uint32_t cmd = 0x0;

	nand_gpbc_reset_buf(gpbc);

	cmd = nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NWPD);
	if (cmd & NAND_GPBC_NWPD__WPD) {
		nand_gpbc_write_byte_to_buf(gpbc, NAND_STATUS_WP);
	} else {
		nand_gpbc_write_byte_to_buf(gpbc, 0);
	}
}

static u32 nand_gpbc_resetBank(struct nand_gpbc_info *gpbc, int bank)
{
	u32		irqMask;
	u32		result;

	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_write32((NAND_GPBC_NDRST__B0RST<<bank), gpbc->regBase + NAND_GPBC_NDRST);
	irqMask = NAND_GPBC_NI__RSTCMP | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & result) {
		return result;
	}

	return 0;
}

static int nand_gpbc_isErasedPageData(struct nand_gpbc_info *gpbc, u8 *buf)
{
	struct nand_chip *chip = &gpbc->chip;
	u32 base, cnt, sect, calc;
	u32 correct_type = nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NECCC);

	calc = 0xffffffff;

	for (sect = 0; sect < chip->ecc.steps; sect++) {
		base = chip->ecc.size * sect;
		cnt = 0;

		if (correct_type == 8) {
			//It is tested in only 8bit Ecc.
			u32 corrected_bit_offset = 0x2c0;
			for (; cnt < corrected_bit_offset;
			     cnt += sizeof(u32)) {
				calc &= *(u32 *)(buf + base + cnt);
			}
			calc &= (*(u32 *)(buf + base + cnt) ^ 0x00000200);
			cnt += sizeof(u32);
		}

		for (; cnt < chip->ecc.size; cnt += sizeof(u32)) {
			calc &= *(u32 *)(buf + base + cnt);
		}
	}

	if (calc != 0xffffffff) {
		return 0;
	}

	return 1;
}

static int nand_gpbc_isErasedPageEcc(struct nand_gpbc_info *gpbc, int page, u8 *buf)
{
	struct nand_chip *chip = &gpbc->chip;
	u32 irq_mask;
	u32 result;
	u32 sect, sect_base, ecc_base, start, end, cnt;
	u8 calc;
	int bank = gpbc->flash_bank;
	u32 bank_sel = bank << NAND_GPBC_BANK_SEL;
	u32 bankPage = page & chip->pagemask;

	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_CMD, NAND_CMD_READ0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_ADR, bankPage & 0xff);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_ADR, (bankPage >> 8) & 0xff);
	if(chip->onfi_params.addr_cycles == 0x23) {
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_ADR, (bankPage >> 16) & 0xff);
	}
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank_sel | NAND_GPBC_MAP11_CMD, NAND_CMD_READSTART);

	irq_mask = NAND_GPBC_NI__INTACT | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irq_mask);
	if (NAND_GPBC_NI__ERR & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}

	calc = 0xff;
	for (sect = 0; sect < chip->ecc.steps; sect++) {
		sect_base = (chip->ecc.size + chip->ecc.bytes) * sect;
		if (sect != chip->ecc.steps - 1) {
			ecc_base = sect_base + chip->ecc.size;
		} else {
			ecc_base = sect_base + gpbc->sectSize2 + gpbc->oobSkipSize + gpbc->sectSize3;
		}

		start = roundup(ecc_base - sizeof(u32) + 1, sizeof(u32));
		end = roundup(ecc_base + chip->ecc.bytes, sizeof(u32));

		for (; start < end; start += sizeof(u32)) {
			*(u32 *)(gpbc->pPageBuf + start) =
				nand_gpbc_index32_read_data(gpbc, NAND_GPBC_MAP00 | bank_sel | start);
		}

		for (cnt = 0; cnt < chip->ecc.bytes; cnt++) {
			calc &= *(gpbc->pPageBuf + ecc_base + cnt);
		}
	}

	if (calc != 0xff) {
		return 0;
	}

	return 1;
}

static int nand_gpbc_isErasedPage(struct nand_gpbc_info *gpbc, int page, u8 *buf)
{
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	struct nand_chip *chip = &gpbc->chip;
	u32 page_offset = page & ((1 << (chip->phys_erase_shift - chip->page_shift)) - 1);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	/* compare DATA */
	if (!nand_gpbc_isErasedPageData(gpbc, buf)) {
		return 0;
	}

	/* compare ECC */
	if (!nand_gpbc_isErasedPageEcc(gpbc, page, buf)) {
		return 0;
	}

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	set_bit_erased_map(page_offset, chip->erased_map);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */
	return 1;
}

static void nand_gpbc_restoreErasedData(struct nand_gpbc_info *gpbc, u8 *pBuf)
{
	struct nand_chip	*chip = &gpbc->chip;
	u32	sect;
	u32 correct_type = nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NECCC);

	if (correct_type == 8) {
		//It is tested in only 8bit Ecc.
		for (sect = 0; sect < chip->ecc.steps; sect++) {
			*(u32 *)(pBuf + chip->ecc.size * sect + 0x2c0) = 0xffffffff;
		}
	}
}

static int nand_gpbc_readPageAllEcc(struct nand_gpbc_info *gpbc, uint8_t *pBuf, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip	*chip = &gpbc->chip;
	u32	irqMask;
	u32	result;
#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
	u32	result2 = 0;
#else /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	u32	cnt;
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	int	bank;
	u32	bankSel;
	u32	bankPage;
	int	max_bitflips = 0;
	int	status;
	u32	dataPos;
	u32	oobPos;
	u32	sect;
	u32	pagePos;
	u32	index_addr;
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	u32 flash_page;
	u16 cmd_flags;
	u64 mem_ptr;
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	nand_gpbc_write32(NAND_GPBC_NTS__TS, gpbc->regBase + NAND_GPBC_NTS);	//spare
	nand_gpbc_set_eccen(gpbc, 0);

#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
	gpbc->pageBufPhys = dma_map_single(gpbc->dev, gpbc->pPageBuf, mtd->writesize + mtd->oobsize, DMA_FROM_DEVICE);
	if(dma_mapping_error(gpbc->dev, gpbc->pageBufPhys)) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		BUG();
	}
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	flash_page = (NAND_GPBC_MAP10 | bankSel | bankPage);
	mem_ptr = (u64)gpbc->pageBufPhys;
	cmd_flags = (NAND_GPBC_CMD_FLAGS_TRANS_MAIN_SPARE |
		     NAND_GPBC_CMD_FLAGS_INT |
		     NAND_GPBC_CMD_FLAGS_BURST_LEN);
	nand_gpbc_set_cmd_desc_read_page(gpbc->cmd_desc, flash_page,
					 cmd_flags, mem_ptr);
#else /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_MAIN_SPARE);	//transfer mode
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
#else /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_MAIN_SPARE);	//transfer mode
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	nand_gpbc_clearIntStatus(gpbc, bank);

#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
	/* enable DMA */
	nand_gpbc_write32(NAND_GPBC_NDME__FLAG, gpbc->regBase + NAND_GPBC_NDME);
	nand_gpbc_set_eccen(gpbc, NAND_GPBC_NECCE__ECCEN);
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	index_addr = NAND_GPBC_MAP10;
	nand_gpbc_index32(gpbc, index_addr, NAND_GPBC_MAP10_BEAT0_CMD_DMA);
	nand_gpbc_index32(gpbc, index_addr, (u32)( (u64)gpbc->cmd_desc_phys        & 0xFFFFFFFF));
	nand_gpbc_index32(gpbc, index_addr, (u32)(((u64)gpbc->cmd_desc_phys >> 32) & 0xFFFFFFFF));
#else /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
	index_addr = NAND_GPBC_MAP10 | bankSel | bankPage;
	nand_gpbc_index32(gpbc, index_addr, NAND_GPBC_MAP10_BEAT0_READ);
	nand_gpbc_index32(gpbc, index_addr, (u32)( (u64)gpbc->pageBufPhys        & 0xFFFFFFFF));
	nand_gpbc_index32(gpbc, index_addr, (u32)(((u64)gpbc->pageBufPhys >> 32) & 0xFFFFFFFF));
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

	irqMask = NAND_GPBC_NI__DMA_CMD_COMP | NAND_GPBC_NI__TOUT;
#else /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, 0x2001);	//read-ahead
	irqMask = NAND_GPBC_NI__LDCMP | NAND_GPBC_NI__ERR;
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */

	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);

#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	result2 = nand_gpbc_poll_cmd_dma(gpbc);
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
	/* disable DMA */
	nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NDME);
	dma_unmap_single(gpbc->dev, gpbc->pageBufPhys, mtd->writesize + mtd->oobsize, DMA_FROM_DEVICE);
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	if (NAND_GPBC_NI__ERR & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}
#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	if (result2 & NAND_GPBC_CDESC_STAT_ERR) {
		pr_err("\n## ERROR %s %s %d: result2 = 0x%08X ##\n",
		       __FILE__, __func__, __LINE__, result2);
		BUG();
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
#else /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	nand_gpbc_set_eccen(gpbc, NAND_GPBC_NECCE__ECCEN);

	index_addr = NAND_GPBC_MAP01 | bankSel | bankPage;
	for (cnt = 0; cnt < mtd->writesize + mtd->oobsize; cnt += 4) {
		*(u32 *)(gpbc->pPageBuf + cnt) = nand_gpbc_index32_read_data(gpbc, index_addr);
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */

	pagePos = 0;
	dataPos = 0;
	oobPos = gpbc->oobSkipSize;
	for (sect = 0; sect < gpbc->chip.ecc.steps - 1; sect++) {
		memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->chip.ecc.size);
		pagePos += gpbc->chip.ecc.size;
		dataPos += gpbc->chip.ecc.size;
		memcpy(gpbc->chip.oob_poi + oobPos, gpbc->pPageBuf + pagePos, gpbc->chip.ecc.bytes);
		pagePos += gpbc->chip.ecc.bytes;
		oobPos += gpbc->chip.ecc.bytes;
	}
	memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->sectSize2);
	pagePos += gpbc->sectSize2;
	dataPos += gpbc->sectSize2;
	memcpy(gpbc->chip.oob_poi, gpbc->pPageBuf + pagePos, gpbc->oobSkipSize);
	pagePos += gpbc->oobSkipSize;
	memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->sectSize3);
	pagePos += gpbc->sectSize3;
	memcpy(gpbc->chip.oob_poi + oobPos, gpbc->pPageBuf + pagePos, mtd->oobsize - gpbc->oobSkipSize - gpbc->sectSize3);

#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	result2 &= NAND_GPBC_CDESC_STAT_FAIL;
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
	result2 |= (result & NAND_GPBC_NI__ECCUNCOR);
	if (result2) {
#else /* CONFIG_MTD_NAND_UNIPHIER_DMA */
	result = nand_gpbc_checkIrq(gpbc, bank);
	if (NAND_GPBC_NI__ECCUNCOR & result) {
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA */
		status = nand_gpbc_isErasedPage(gpbc, bankPage, pBuf);
		if (status) {
			nand_gpbc_restoreErasedData(gpbc, pBuf);
		} else {
			mtd->ecc_stats.failed++;
			pr_warn("\n\n## Warning ## %s %s %d: ECCUNCOR ##\n\n", __FILE__, __func__, __LINE__);
		}
	} else {
		max_bitflips = nand_gpbc_get_max_bitflips(gpbc, bank);
		if (mtd->bitflip_threshold <= max_bitflips) {
			mtd->ecc_stats.corrected++;
		}
	}

	return max_bitflips;
}

static u32 nand_gpbc_readPageAllRaw(struct nand_gpbc_info *gpbc,
				    uint8_t *pBuf, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip *chip = &gpbc->chip;
	u32	irqMask;
	u32	result;
	u32	cnt;
	int	bank;
	u32	bankSel;
	u32	dataPos;
	u32	oobPos;
	u32	sect;
	u32	pagePos;
	u32	bankPage;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_CMD, NAND_CMD_READ0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, bankPage & 0xff);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, (bankPage >> 8) & 0xff);
	if(chip->onfi_params.addr_cycles == 0x23) {
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, (bankPage >> 16) & 0xff);
	}
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_CMD, NAND_CMD_READSTART);

	irqMask = NAND_GPBC_NI__INTACT | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & result) {
		pr_warn("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}
	for (cnt = 0; cnt < mtd->writesize + mtd->oobsize; cnt += 4) {
		*(u32 *)(gpbc->pPageBuf + cnt) = nand_gpbc_index32_read_data(gpbc, NAND_GPBC_MAP00 | bankSel | cnt);
	}

	pagePos = 0;
	dataPos = 0;
	oobPos = gpbc->oobSkipSize;
	for (sect = 0; sect < gpbc->chip.ecc.steps - 1; sect++) {
		memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->chip.ecc.size);
		pagePos += gpbc->chip.ecc.size;
		dataPos += gpbc->chip.ecc.size;
		memcpy(gpbc->chip.oob_poi + oobPos, gpbc->pPageBuf + pagePos, gpbc->chip.ecc.bytes);
		pagePos += gpbc->chip.ecc.bytes;
		oobPos += gpbc->chip.ecc.bytes;
	}
	memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->sectSize2);
	pagePos += gpbc->sectSize2;
	dataPos += gpbc->sectSize2;
	memcpy(gpbc->chip.oob_poi, gpbc->pPageBuf + pagePos, gpbc->oobSkipSize);
	pagePos += gpbc->oobSkipSize;
	memcpy(pBuf + dataPos, gpbc->pPageBuf + pagePos, gpbc->sectSize3);
	pagePos += gpbc->sectSize3;
	memcpy(gpbc->chip.oob_poi + oobPos, gpbc->pPageBuf + pagePos, mtd->oobsize - gpbc->oobSkipSize - gpbc->sectSize3);

	return 0;
}

static int nand_gpbc_writePageAllEcc(struct nand_gpbc_info *gpbc,
				     const uint8_t *pBuf, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip	*chip = &gpbc->chip;
	u32	dataPos;
	u32	oobPos;
	u32	sect;
	u32	pagePos;
	u32	irqMask;
	u32	result;
	int	bank;
	u32	bankSel;
	u32	bankPage;
	u32	index_addr;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	pagePos = 0;
	dataPos = 0;
	oobPos = gpbc->oobSkipSize;
	for (sect = 0; sect < gpbc->chip.ecc.steps - 1; sect++) {
		memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->chip.ecc.size);
		pagePos += gpbc->chip.ecc.size;
		dataPos += gpbc->chip.ecc.size;
		memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi + oobPos, gpbc->chip.ecc.bytes);
		pagePos += gpbc->chip.ecc.bytes;
		oobPos += gpbc->chip.ecc.bytes;
	}
	memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->sectSize2);
	pagePos += gpbc->sectSize2;
	dataPos += gpbc->sectSize2;
	memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi, gpbc->oobSkipSize);
	pagePos += gpbc->oobSkipSize;
	memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->sectSize3);
	pagePos += gpbc->sectSize3;
	memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi + oobPos, mtd->oobsize - gpbc->oobSkipSize - gpbc->sectSize3);

	nand_gpbc_write32(NAND_GPBC_NTS__TS, gpbc->regBase + NAND_GPBC_NTS);	//spare
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_MAIN_SPARE);	//transfer mode
	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_set_eccen(gpbc, NAND_GPBC_NECCE__ECCEN);

#ifdef CONFIG_MTD_NAND_UNIPHIER_DMA
	gpbc->pageBufPhys = dma_map_single(gpbc->dev, gpbc->pPageBuf, mtd->writesize + mtd->oobsize, DMA_TO_DEVICE);
	if(dma_mapping_error(gpbc->dev, gpbc->pageBufPhys)) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		BUG();
	}
	/* enable DMA */
	nand_gpbc_write32(NAND_GPBC_NDME__FLAG, gpbc->regBase + NAND_GPBC_NDME);

	index_addr = NAND_GPBC_MAP10 | bankSel | bankPage;
	nand_gpbc_index32(gpbc, index_addr, NAND_GPBC_MAP10_BEAT0_WRITE);
	nand_gpbc_index32(gpbc, index_addr, (u32)( (u64)gpbc->pageBufPhys        & 0xFFFFFFFF));
	nand_gpbc_index32(gpbc, index_addr, (u32)(((u64)gpbc->pageBufPhys >> 32) & 0xFFFFFFFF));

	irqMask = NAND_GPBC_NI__DMA_CMD_COMP | NAND_GPBC_NI__TOUT;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);

	/* disable DMA */
	nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NDME);
	dma_unmap_single(gpbc->dev, gpbc->pageBufPhys, mtd->writesize + mtd->oobsize, DMA_TO_DEVICE);
#else
	index_addr = NAND_GPBC_MAP01 | bankSel | bankPage;
	for (pagePos = 0; pagePos < mtd->writesize + mtd->oobsize; pagePos += 4) {
		nand_gpbc_index32(gpbc, index_addr, *(u32 *)(gpbc->pPageBuf + pagePos));
	}

	irqMask = NAND_GPBC_NI__PROCMP | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
#endif

	if (NAND_GPBC_NI__ERR & ~NAND_GPBC_NI__PROFAIL & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}

	gpbc->status = (result & NAND_GPBC_NI__PROFAIL) ? NAND_STATUS_FAIL : 0;

	return 0;
}

static int nand_gpbc_writePageAllRaw(struct nand_gpbc_info *gpbc,
				      const uint8_t *pBuf, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip	*chip = &gpbc->chip;
	u32	dataPos;
	u32	oobPos;
	u32	sect;
	u32	pagePos;
	u32	irqMask;
	u32 result;
	int	bank;
	u32	bankSel;
	u32	bankPage;
	u32	cnt;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	pagePos = 0;
	dataPos = 0;
	oobPos = gpbc->oobSkipSize;
	for (sect = 0; sect < gpbc->chip.ecc.steps - 1; sect++) {
		memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->chip.ecc.size);
		pagePos += gpbc->chip.ecc.size;
		dataPos += gpbc->chip.ecc.size;
		memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi + oobPos, gpbc->chip.ecc.bytes);
		pagePos += gpbc->chip.ecc.bytes;
		oobPos += gpbc->chip.ecc.bytes;
	}
	memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->sectSize2);
	pagePos += gpbc->sectSize2;
	dataPos += gpbc->sectSize2;
	memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi, gpbc->oobSkipSize);
	pagePos += gpbc->oobSkipSize;
	memcpy(gpbc->pPageBuf + pagePos, pBuf + dataPos, gpbc->sectSize3);
	pagePos += gpbc->sectSize3;
	memcpy(gpbc->pPageBuf + pagePos, gpbc->chip.oob_poi + oobPos, mtd->oobsize - gpbc->oobSkipSize - gpbc->sectSize3);

	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_CMD, NAND_CMD_SEQIN);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, bankPage & 0xff);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, (bankPage >> 8) & 0xff);
	if(chip->onfi_params.addr_cycles == 0x23) {
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_ADR, (bankPage >> 16) & 0xff);
	}
	for (cnt = 0; cnt < mtd->writesize + mtd->oobsize; cnt += 4) {
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP00 | bankSel | cnt, *(u32 *)(gpbc->pPageBuf + cnt));
	}
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bankSel | NAND_GPBC_MAP11_CMD, NAND_CMD_PAGEPROG);

	irqMask = NAND_GPBC_NI__INTACT | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & ~NAND_GPBC_NI__PROFAIL & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}

	gpbc->status = (result & NAND_GPBC_NI__PROFAIL) ? NAND_STATUS_FAIL : 0;

	return 0;
}

static int nand_gpbc_readPageOob(struct nand_gpbc_info *gpbc, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip	*chip = &gpbc->chip;
	u32	irqMask;
	u32	result;
	u32	cnt;
	u8	*pBuf;
	int	bank;
	u32	bankSel;
	u32	bankPage;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	nand_gpbc_write32(NAND_GPBC_NTS__TS, gpbc->regBase + NAND_GPBC_NTS);	//spare
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_SPARE);	//transfer mode
	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, 0x2001);	//read-ahead

	irqMask = NAND_GPBC_NI__LDCMP | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}
	pBuf = chip->oob_poi;
	for (cnt = 0; cnt < mtd->oobsize; cnt += 4) {
		*(u32 *)(pBuf + cnt) = nand_gpbc_index32_read_data(gpbc, NAND_GPBC_MAP01 | bankSel | bankPage);
	}

	return 0;
}

static int nand_gpbc_writePageOob(struct nand_gpbc_info *gpbc, int page)
{
	struct mtd_info		*mtd = &gpbc->mtd;
	struct nand_chip	*chip = &gpbc->chip;
	u32	irqMask;
	u32	result;
	u32	cnt;
	int	bank;
	u32	bankSel;
	u32	bankPage;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	nand_gpbc_write32(NAND_GPBC_NTS__TS, gpbc->regBase + NAND_GPBC_NTS);	//spare
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_SPARE);	//transfer mode
	nand_gpbc_clearIntStatus(gpbc, bank);
	for (cnt = 0; cnt < mtd->oobsize; cnt += 4) {
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP01 | bankSel | bankPage, *(u32 *)(gpbc->chip.oob_poi + cnt));
	}

	irqMask = NAND_GPBC_NI__PROCMP | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & ~NAND_GPBC_NI__PROFAIL & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	} else if (NAND_GPBC_NI__PROFAIL & result) {
		return -EIO;
	}

	return 0;
}

//----------------------------
//-------- ecc func ----------
//----------------------------
static void nand_gpbc_hwctl(struct mtd_info *mtd, int mode)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_hwctl called unexpectedly\n");
	BUG();
}

static int nand_gpbc_calculate(struct mtd_info *mtd, const uint8_t *dat,
			uint8_t *ecc_code)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_calculate called unexpectedly\n");
	BUG();
	return -EIO;
}

static int nand_gpbc_correct(struct mtd_info *mtd, uint8_t *dat, uint8_t *read_ecc,
		      uint8_t *calc_ecc)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_correct called unexpectedly\n");
	BUG();
	return -EIO;
}

static int nand_gpbc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				   uint8_t *buf, int oob_required, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_readPageAllRaw(gpbc, buf, page);
}

static int nand_gpbc_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				     const uint8_t *buf, int oob_required, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_writePageAllRaw(gpbc, buf, page);
}

static int nand_gpbc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			       uint8_t *buf, int oob_required, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_readPageAllEcc(gpbc, buf, page);
}

static int nand_gpbc_read_subpage(struct mtd_info *mtd, struct nand_chip *chip,
				  uint32_t offs, uint32_t len, uint8_t *buf, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev, "nand_gpbc_read_subpage called unexpectedly\n");
	BUG();
	return 0;
}

static int nand_gpbc_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf, int oob_required, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_writePageAllEcc(gpbc, buf, page);
}

static int nand_gpbc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_readPageOob(gpbc, page);
}

static int nand_gpbc_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	return nand_gpbc_writePageOob(gpbc, page);
}


//----------------------------
//------- chip func ----------
//----------------------------
static uint8_t nand_gpbc_read_byte(struct mtd_info *mtd)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	uint32_t result;
	u32 bank = gpbc->flash_bank << NAND_GPBC_BANK_SEL;

	if (gpbc->buf.head < gpbc->buf.tail) {
		result = gpbc->buf.buf[gpbc->buf.head++];
	} else {
		result = nand_gpbc_index32_read_data(gpbc, NAND_GPBC_MAP11 | bank | NAND_GPBC_MAP11_DAT);
	}
	return (uint8_t)result & 0xFF;
}

static u16 nand_gpbc_read_word(struct mtd_info *mtd)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_read_word called unexpectedly\n");
	BUG();
	return 0;
}

static void nand_gpbc_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_write_buf called unexpectedly\n");
	BUG();
}

static void nand_gpbc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_read_buf called unexpectedly\n");
	BUG();
}

static void nand_gpbc_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	gpbc->flash_bank = chip;
}

static void nand_gpbc_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	dev_err(gpbc->dev,
		"nand_gpbc_cmd_ctrl called unexpectedly\n");
	BUG();
}

static void nand_gpbc_cmdfunc(struct mtd_info *mtd, unsigned int cmd, int col,
			      int page)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	u32 bank = gpbc->flash_bank << NAND_GPBC_BANK_SEL;
	u32 result;

	switch (cmd) {
	case NAND_CMD_PAGEPROG:
		break;
	case NAND_CMD_STATUS:
		nand_gpbc_read_status(gpbc);
		break;
	case NAND_CMD_PARAM:
		nand_gpbc_reset_buf(gpbc);

		nand_gpbc_clearIntStatus(gpbc, bank); 
		nand_gpbc_set_eccen(gpbc, 0);
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank | NAND_GPBC_MAP11_CMD, cmd);
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank | NAND_GPBC_MAP11_ADR, col);

		result = nand_gpbc_waitIrq(gpbc, bank, NAND_GPBC_NI__INTACT | NAND_GPBC_NI__ERR);
		if (NAND_GPBC_NI__ERR & result) {
			pr_warn("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
			BUG();
		}
		break;
	case NAND_CMD_READID:
		nand_gpbc_reset_buf(gpbc);

		nand_gpbc_set_eccen(gpbc, 0);
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank | NAND_GPBC_MAP11_CMD, cmd);
		nand_gpbc_index32(gpbc, NAND_GPBC_MAP11 | bank | NAND_GPBC_MAP11_ADR, col);
		break;
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
		gpbc->page = page;
		break;
	case NAND_CMD_RESET:
		nand_gpbc_resetBank(gpbc, gpbc->flash_bank);
		break;
	case NAND_CMD_READOOB:
		/* TODO: Read OOB data */
		break;
	default:
		pr_err(": unsupported command received 0x%x\n", cmd);
		break;
	}
}

static int nand_gpbc_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	int status = gpbc->status;

	gpbc->status = 0;
	return status;
}

static int nand_gpbc_erase(struct mtd_info *mtd, int page)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_gpbc_info *gpbc = mtd_to_gpbc(mtd);
	u32 irqMask;
	u32 result;
	int bank;
	u32 bankSel;
	u32 bankPage;

	bank = gpbc->flash_bank;
	bankSel = bank << NAND_GPBC_BANK_SEL;
	bankPage = page & chip->pagemask;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	bankPage = bbt_translateBb(mtd, bankPage, bank);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	nand_gpbc_clearIntStatus(gpbc, bank);
	nand_gpbc_set_eccen(gpbc, 0);
	nand_gpbc_index32(gpbc, NAND_GPBC_MAP10 | bankSel | bankPage, NAND_GPBC_MAP10_ERASE);

	irqMask = NAND_GPBC_NI__ERACMP | NAND_GPBC_NI__ERR;
	result = nand_gpbc_waitIrq(gpbc, bank, irqMask);
	if (NAND_GPBC_NI__ERR & ~NAND_GPBC_NI__ERAFAIL & result) {
		pr_err("\n## ERROR %s %s %d: result = 0x%08X ##\n", __FILE__, __func__, __LINE__, result);
		BUG();
	}

	gpbc->status = (result & NAND_GPBC_NI__ERAFAIL) ? NAND_STATUS_FAIL : 0;

	return nand_gpbc_waitfunc(mtd, chip);
}


//----------------------------
//-------- init func ---------
//----------------------------
static void nand_gpbc_initHw1(struct nand_gpbc_info *gpbc)
{
	u32 temp32;

	if(!gpbc->soc_flags.ecc_sw_ctrl) {
		temp32 = nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NECCE);
		if (!(temp32 & NAND_GPBC_NECCE__ECCEN)) {
			pr_err("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
			BUG();
		}
	}

	/* disable WP */
	nand_gpbc_write32(NAND_GPBC_NWPD__WPD, gpbc->regBase + NAND_GPBC_NWPD);

	/* Enable R/B pin of all banks */
	nand_gpbc_write32(NAND_GPBC_NRBE__MASK, gpbc->regBase + NAND_GPBC_NRBE);

	/* set correction value per ECC sector */
	nand_gpbc_write32(CONFIG_MTD_NAND_UNIPHIER_ECC_CORRECT_BITS,
			  gpbc->regBase + NAND_GPBC_NECCC);

#if 0
	/* Set AC timing (initial value) */
	nand_gpbc_write32(0x1432, gpbc->regBase + NAND_GPBC_NCDWR);
	nand_gpbc_write32(0x1432, gpbc->regBase + NAND_GPBC_NCAAD);
	nand_gpbc_write32(0x0032, gpbc->regBase + NAND_GPBC_NRW);
	nand_gpbc_write32(0x0000, gpbc->regBase + NAND_GPBC_NRDCP);
	nand_gpbc_write32(0x0012, gpbc->regBase + NAND_GPBC_NRWLP);
	nand_gpbc_write32(0x000c, gpbc->regBase + NAND_GPBC_NRWHP);
	nand_gpbc_write32(0xa003, gpbc->regBase + NAND_GPBC_NCES);
	nand_gpbc_write32(0x0032, gpbc->regBase + NAND_GPBC_NRR);
#endif
}

static void nand_gpbc_initHw2(struct nand_gpbc_info *gpbc)
{
	struct mtd_info *mtd = &gpbc->mtd;
	struct nand_chip *chip = &gpbc->chip;
	struct nand_gpbc_timing *timing;
	u32 temp32;

	/* Set SPARE_AREA_SKIP_BYTES */
	nand_gpbc_write32(NAND_GPBC_OOB_SKIP_SIZE, gpbc->regBase + NAND_GPBC_NSASK);

	/* Set SPARE_AREA_MARKER */
	nand_gpbc_write32(0xffff, gpbc->regBase + NAND_GPBC_NSAMK);

	/* Set PAGES_PER_BLOCK */
	temp32 = mtd->erasesize / mtd->writesize;
	nand_gpbc_write32(temp32, gpbc->regBase + NAND_GPBC_NBPNUM);

	/* Set DEVICE_MAIN_AREA_SIZE */
	nand_gpbc_write32(mtd->writesize, gpbc->regBase + NAND_GPBC_NMASZ);

	/* Set DEVICE_SPARE_AREA_SIZE */
	nand_gpbc_write32(mtd->oobsize, gpbc->regBase + NAND_GPBC_NSASZ);

#if defined(CONFIG_MTD_NAND_UNIPHIER_DMA) && !defined(CONFIG_MTD_NAND_UNIPHIER_CDMA_READ)
	if(gpbc->soc_flags.ddma_comp_fix) {
		/* When you use Data DMA, set the psc[0] bit. */
		temp32 = nand_gpbc_read32(gpbc->regBase + NAND_GPBC_NFBLEN);
		temp32 |= NAND_GPBC_NFBLEN__PSC0;
		nand_gpbc_write32(temp32, gpbc->regBase + NAND_GPBC_NFBLEN);
	} else {
		pr_err("\n## ERROR %s %s %d: cannot fix Data-DMA \"x8\" ##\n",
		       __FILE__, __func__, __LINE__);
		BUG();
	}
#endif

	/* Set CFG_NUM_DATA_BLOCKS */
	if(gpbc->soc_flags.set_ndb) {
		temp32 = mtd->writesize / NAND_GPBC_ECCSIZE;
		nand_gpbc_write32(temp32, gpbc->regBase + NAND_GPBC_NDB);
	}

	/* Set CHIP_INTERLEAVE_ENABLE */
	if(gpbc->soc_flags.set_cdma_erren) {
		nand_gpbc_write32(NAND_GPBC_NCIEAIR__AIR, gpbc->regBase + NAND_GPBC_NCIEAIR);
	}

	/* Set MANUFACTURE_ID */
	nand_gpbc_write32(chip->onfi_params.jedec_id, gpbc->regBase + NAND_GPBC_NMID);

	/* Set DEVICE_WIDTH */
	if (!(chip->options & NAND_BUSWIDTH_16)) {
		nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NDW);
	} else {
		pr_err("\n## ERROR %s %s %d: supported organization is only \"x8\" ##\n",
		       __FILE__, __func__, __LINE__);
		BUG();
	}

	/* Set DEVICE_NUMBER */
	nand_gpbc_write32(0x00000001, gpbc->regBase + NAND_GPBC_NDNUM);

	/* Set Address cycle */
	temp32 = 0;
	if(chip->onfi_params.addr_cycles == 0x22) {
		temp32 = NAND_GPBC_NTRADC__TRADC;
	}
	nand_gpbc_write32(temp32, gpbc->regBase + NAND_GPBC_NTRADC);

	/* Set AC timing */
	timing = nand_gpbc_get_timing(gpbc);
	if(timing) {
		nand_gpbc_write32(timing->ncdwr, gpbc->regBase + NAND_GPBC_NCDWR);
		nand_gpbc_write32(timing->ncaad, gpbc->regBase + NAND_GPBC_NCAAD);
		nand_gpbc_write32(timing->nrw  , gpbc->regBase + NAND_GPBC_NRW);
		nand_gpbc_write32(timing->nrdcp, gpbc->regBase + NAND_GPBC_NRDCP);
		nand_gpbc_write32(timing->nrwlp, gpbc->regBase + NAND_GPBC_NRWLP);
		nand_gpbc_write32(timing->nrwhp, gpbc->regBase + NAND_GPBC_NRWHP);
		nand_gpbc_write32(timing->nces , gpbc->regBase + NAND_GPBC_NCES);
		nand_gpbc_write32(timing->nrr  , gpbc->regBase + NAND_GPBC_NRR);
	}
}


#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM

/* AREA_ORDER {BOOT_ORDER, DATA_ORDER, ALT_ORDER, BBM_ORDER} */
#define NAND_MN2WS_AREA_ORDER_LASTBLOCK \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_DATA, NAND_AREA_ID_ALT, NAND_AREA_ID_BBM}
#define NAND_MN2WS_AREA_ORDER_FIRSTBLOCK \
	{NAND_AREA_ID_BBM, NAND_AREA_ID_ALT, NAND_AREA_ID_DATA, NAND_AREA_ID_BOOT}
#ifdef NAND_MN2WS_AREA_ORDER
#if (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BDAM)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_DATA, NAND_AREA_ID_ALT, NAND_AREA_ID_BBM}
#elif (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BDMA)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_DATA, NAND_AREA_ID_BBM, NAND_AREA_ID_ALT}
#elif (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BADM)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_ALT, NAND_AREA_ID_DATA, NAND_AREA_ID_BBM}
#elif (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BAMD)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_ALT, NAND_AREA_ID_BBM, NAND_AREA_ID_DATA}
#elif (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BMDA)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_BBM, NAND_AREA_ID_DATA, NAND_AREA_ID_ALT}
#elif (NAND_MN2WS_AREA_ORDER == NAND_MN2WS_AREA_ORDER_BMAD)
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE \
	{NAND_AREA_ID_BOOT, NAND_AREA_ID_BBM, NAND_AREA_ID_ALT, NAND_AREA_ID_DATA}
#else
#error Error: Invalid  NAND_MN2WS_AREA_ORDER.
#endif
#else /* NAND_MN2WS_AREA_ORDER */
/* dummy: not used NAND_MN2WS_AREA_ORDER_FLEXIBLE */
#define NAND_MN2WS_AREA_ORDER_FLEXIBLE NAND_MN2WS_AREA_ORDER_LASTBLOCK
#endif /* NAND_MN2WS_AREA_ORDER */

#define SET_AREA_ORDER(dst, order)		\
do {						\
	typeof(dst) __temp = order;		\
	memcpy(dst, __temp, sizeof(dst));	\
} while (0)

static int nand_gpbc_setAreaInfo(struct nand_chip *chip,
				 struct nand_bbt_area_info *area_info,
				 u32 areaBlocks[][NAND_MAX_CHIPS], u32 chip_num)
{
	u32 area, block, size, i;
	u32 chipBlocks = 1 << (chip->chip_shift - chip->bbt_erase_shift);
	u32 order[NAND_AREA_ID_NUM];

	if ((chip->bbt_td->options ^ chip->bbt_md->options) &
	    (NAND_BBT_FLEXIBLE | NAND_BBT_LASTBLOCK)) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -EINVAL;
	}

	if (chip->bbt_td->options & NAND_BBT_FLEXIBLE) {
		SET_AREA_ORDER(order, NAND_MN2WS_AREA_ORDER_FLEXIBLE);
	} else if (chip->bbt_td->options & NAND_BBT_LASTBLOCK) {
		SET_AREA_ORDER(order, NAND_MN2WS_AREA_ORDER_LASTBLOCK);
	} else {
		SET_AREA_ORDER(order, NAND_MN2WS_AREA_ORDER_FIRSTBLOCK);
	}

	block = chipBlocks * chip_num;
	for (i = 0; i < NAND_AREA_ID_NUM; i++) {
		area = order[i];
		size = areaBlocks[area][chip_num];
		area_info[area].startBlock = block;
		area_info[area].blockNum = size;
		block += size;
	}

	return 0;
}

static u32 nand_gpbc_get_dt_property(struct nand_gpbc_info *gpbc, char *prop)
{
	u32 val;

	if(of_property_read_u32(gpbc->dev->of_node, prop, &val)) {
		val = 0;
	}
	return val;
}

static int nand_gpbc_allocBbmResources(struct nand_gpbc_info *gpbc)
{
	u32		cnt;
	u32		chipBlocks = 1 << (gpbc->chip.chip_shift - gpbc->chip.bbt_erase_shift);
	/* {{BOOT}, {DATA}, {ALT}, {BBM}} */
	u32 areaBlocks[NAND_AREA_ID_NUM][NAND_MAX_CHIPS] = {{0},{0},{0},{0}};

	areaBlocks[NAND_AREA_ID_BOOT][0] = nand_gpbc_get_dt_property(gpbc, "boot_block_bank0");
	areaBlocks[NAND_AREA_ID_BOOT][1] = nand_gpbc_get_dt_property(gpbc, "boot_block_bank1");
	areaBlocks[NAND_AREA_ID_ALT][0] = nand_gpbc_get_dt_property(gpbc, "alt_block_bank0");
	areaBlocks[NAND_AREA_ID_ALT][1] = nand_gpbc_get_dt_property(gpbc, "alt_block_bank1");

	for (cnt = 0; cnt < gpbc->chip.numchips; cnt++) {
		areaBlocks[NAND_AREA_ID_BBM][cnt] = NAND_BBM_BLOCK_NUM;
		areaBlocks[NAND_AREA_ID_DATA][cnt] = chipBlocks -
			(areaBlocks[NAND_AREA_ID_BOOT][cnt] + areaBlocks[NAND_AREA_ID_ALT][cnt] +
			 areaBlocks[NAND_AREA_ID_BBM][cnt]);
	}

	gpbc->chip.pBbmBuf = NULL;
	gpbc->chip.pBlockBuf = NULL;
	gpbc->chip.psBbm = nand_gpbc_ABbm;
	for (cnt = 0; cnt < NAND_MAX_CHIPS; cnt++) {
		struct nand_bbm		*psBbm;

		psBbm = &gpbc->chip.psBbm[cnt];
		psBbm->mtBlocks = areaBlocks[NAND_AREA_ID_ALT][cnt] +
			areaBlocks[NAND_AREA_ID_BBM][cnt];
		psBbm->bbmPages = -1;
		psBbm->bbMapSize = -1;
		psBbm->bbListSize = -1;
		psBbm->usedMtBlocks = -1;
		psBbm->altBlocks = -1;
		psBbm->pBbMap = NULL;
		psBbm->psBbList = NULL;
		psBbm->area = NULL;
	}

	for (cnt = 0; cnt < gpbc->chip.numchips; cnt++) {
		struct nand_bbm		*psBbm;

		psBbm = &gpbc->chip.psBbm[cnt];
		psBbm->bbMapSize = ((chipBlocks + 8 * 4 - 1) / (8 * 4)) * 4;
		psBbm->bbListSize = sizeof(struct nand_bbList) * psBbm->mtBlocks;
		if (gpbc->chip.bbt_td->options & NAND_BBT_NO_OOB) {
			psBbm->bbmPages = (psBbm->bbMapSize + psBbm->bbListSize + 4 * 2
					   + sizeof(gpbc->chip.bbt_td->pages[0])
					   + sizeof(gpbc->chip.bbt_md->pages[0])
					   + (1 << gpbc->chip.page_shift)
					   + sizeof(gpbc->chip.bbt_td->len)
					   + roundup(sizeof(gpbc->chip.bbt_td->version[0]), sizeof(u32)) - 1)
				/ (1 << gpbc->chip.page_shift);
		} else {
			psBbm->bbmPages = (psBbm->bbMapSize + psBbm->bbListSize + 4 * 2
					   + sizeof(gpbc->chip.bbt_td->pages[0])
					   + sizeof(gpbc->chip.bbt_md->pages[0])
					   + (1 << gpbc->chip.page_shift) - 1)
				/ (1 << gpbc->chip.page_shift);
		}
		psBbm->pBbMap = kmalloc(psBbm->bbMapSize, GFP_KERNEL);
		if (!psBbm->pBbMap) {
			return -ENOMEM;
		}
		psBbm->psBbList = kmalloc(psBbm->bbListSize, GFP_KERNEL);
		if (!psBbm->psBbList) {
			return -ENOMEM;
		}
		psBbm->area = vmalloc(sizeof(struct nand_bbt_area_info) * NAND_AREA_ID_NUM);
		if (!psBbm->area) {
			return -ENOMEM;
		}
		if (nand_gpbc_setAreaInfo(&gpbc->chip, psBbm->area, areaBlocks, cnt)) {
			return -EINVAL;
		}
	}

	gpbc->chip.pBbmBuf = vmalloc(gpbc->chip.psBbm[0].bbmPages * (gpbc->mtd.writesize + gpbc->mtd.oobsize));
	if (!gpbc->chip.pBbmBuf) {
		return -ENOMEM;
	}

	return 0;
}

static void nand_gpbc_freeBbmResources(struct nand_gpbc_info *gpbc)
{
	u32		cnt;

	if (gpbc->chip.pBbmBuf) {
		vfree(gpbc->chip.pBbmBuf);
	}
	for (cnt = 0; cnt < gpbc->chip.numchips; cnt++) {
		struct nand_bbm		*psBbm;

		psBbm = &gpbc->chip.psBbm[cnt];
		if (psBbm->pBbMap) {
			kfree(psBbm->pBbMap);
		}
		if (psBbm->psBbList) {
			kfree(psBbm->psBbList);
		}
	}
}

static int nand_gpbc_alloc_resources(struct nand_gpbc_info *gpbc)
{
	int pages_per_block = 1 << (gpbc->chip.bbt_erase_shift - gpbc->chip.page_shift);
	size_t size;
	int ret = 0;

	ret = nand_gpbc_allocBbmResources(gpbc);
	if (ret) {
		return ret;
	}

	size = pages_per_block * (gpbc->mtd.writesize + gpbc->mtd.oobsize);
	gpbc->chip.pBlockBuf = vmalloc(size);
	if (!gpbc->chip.pBlockBuf) {
		return -ENOMEM;
	}

	size = DIV_ROUND_UP(pages_per_block, 32) * sizeof(u32);
	gpbc->chip.erased_map = vmalloc(size);
	if (!gpbc->chip.erased_map) {
		return -ENOMEM;
	}
	clear_erased_map(&gpbc->chip);

	return 0;
}

static void nand_gpbc_free_resources(struct nand_gpbc_info *gpbc)
{
	if (gpbc->chip.erased_map) {
		vfree(gpbc->chip.erased_map);
	}

	if (gpbc->chip.pBlockBuf) {
		vfree(gpbc->chip.pBlockBuf);
	}

	nand_gpbc_freeBbmResources(gpbc);
}
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

static int nand_gpbc_probe(struct platform_device *dev)
{
	struct resource *nand_reg, *nand_data;
	struct mtd_part_parser_data ppdata;
	struct nand_gpbc_info *gpbc;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int err = 0;
	u32 temp32;

	gpbc = devm_kzalloc(&dev->dev, sizeof(*gpbc), GFP_KERNEL);
	if (!gpbc) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}
	platform_set_drvdata(dev, gpbc);

	mtd = &gpbc->mtd;
	chip = &gpbc->chip;

	gpbc->dev = &dev->dev;
	gpbc->nist[0] = NAND_GPBC_NIST0;
	gpbc->nist[1] = NAND_GPBC_NIST1;
	gpbc->nie[0] = NAND_GPBC_NIE0;
	gpbc->nie[1] = NAND_GPBC_NIE1;

	nand_gpbc_get_soc_restrict(gpbc);

	nand_reg = platform_get_resource_byname(dev, IORESOURCE_MEM, "nand_reg");
	gpbc->regBase = devm_ioremap_resource(gpbc->dev, nand_reg);
        if (IS_ERR(gpbc->regBase)) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
                return PTR_ERR(gpbc->regBase);
	}
	nand_data = platform_get_resource_byname(dev, IORESOURCE_MEM, "nand_data");
	gpbc->memBase = devm_ioremap_resource(gpbc->dev, nand_data);
        if (IS_ERR(gpbc->memBase)) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
                return PTR_ERR(gpbc->memBase);
	}

#ifdef CONFIG_MTD_NAND_UNIPHIER_IRQ
	gpbc->irq = platform_get_irq(dev, 0);
	if(gpbc->irq < 0) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return gpbc->irq;
	}

	init_completion(&gpbc->complete);
	nand_gpbc_write32(0, gpbc->regBase + gpbc->nie[0]);
	nand_gpbc_write32(0, gpbc->regBase + gpbc->nie[1]);
	nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NDMIE);
	nand_gpbc_write32(NAND_GPBC_NGINTE__INTEN, gpbc->regBase + NAND_GPBC_NGINTE);
	err = devm_request_threaded_irq(gpbc->dev, gpbc->irq, nand_gpbc_irq, NULL, 0, "nand-gpbc", gpbc);
	if(err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return err;
	}
#endif

	nand_gpbc_initHw1(gpbc);

	mtd->owner = THIS_MODULE;
	mtd->priv = &gpbc->chip;
	mtd->bitflip_threshold = NAND_GPBC_BITFLIP_THRESHOLD;

	chip->priv = gpbc;
	chip->IO_ADDR_R = 0;
	chip->IO_ADDR_W = 0;

	chip->read_byte = nand_gpbc_read_byte;
	chip->read_word = nand_gpbc_read_word;
	chip->write_buf = nand_gpbc_write_buf;
	chip->read_buf = nand_gpbc_read_buf;
	chip->select_chip = nand_gpbc_select_chip;
	chip->cmd_ctrl = nand_gpbc_cmd_ctrl;
	chip->cmdfunc = nand_gpbc_cmdfunc;
	chip->waitfunc = nand_gpbc_waitfunc;
	chip->bbt_options = NAND_GPBC_BBT_OPTIONS;
	chip->options = NAND_GPBC_OPTIONS;
	chip->chip_delay = 0;	//unused param

	/* allocate a temporary buffer for nand_scan_ident() */
	gpbc->buf.buf = devm_kzalloc(gpbc->dev, PAGE_SIZE, GFP_KERNEL);
	if (!gpbc->buf.buf) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}

	err = nand_scan_ident(&gpbc->mtd, NAND_GPBC_FLASH_BANKS, NULL);
	if (err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENXIO;
	}

	/* overwrite nand_chip structures */
	chip->erase = nand_gpbc_erase;
	chip->badblockbits = 1; /* unused param */

	/* check ONFI */
	if(!chip->onfi_version) {
		pr_err("\n## ERROR %s %s %d: not supported ONFI! ##\n",
		       __FILE__, __func__, __LINE__);
		BUG();
	}

	/* allocate the right size buffer now */
	devm_kfree(gpbc->dev, gpbc->buf.buf);
	gpbc->buf.buf = devm_kzalloc(gpbc->dev, mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!gpbc->buf.buf) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}

#ifndef CONFIG_MTD_NAND_UNIPHIER_DMA
	gpbc->pPageBuf = devm_kzalloc(gpbc->dev, mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!gpbc->pPageBuf) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}
#else
	if (NAND_GPBC_DMA_BURST < cache_line_size()) {
		if (cache_line_size() % NAND_GPBC_DMA_BURST)
			BUG();
		gpbc->dmaAlignSize = cache_line_size();
	} else {
		if (NAND_GPBC_DMA_BURST % cache_line_size())
			BUG();
		gpbc->dmaAlignSize = NAND_GPBC_DMA_BURST;
	}
	gpbc->pPageBufRaw = devm_kzalloc(gpbc->dev,
					 ALIGN(mtd->writesize + mtd->oobsize, gpbc->dmaAlignSize) + gpbc->dmaAlignSize,
					 GFP_KERNEL);
	if (!gpbc->pPageBufRaw) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}

	gpbc->pPageBuf = (void *)ALIGN((u64)gpbc->pPageBufRaw, gpbc->dmaAlignSize);

#ifdef CONFIG_MTD_NAND_UNIPHIER_CDMA_READ
	gpbc->cmd_desc = dma_alloc_coherent(gpbc->dev,
					  sizeof(struct nand_gpbc_cmd_desc_t),
					  gpbc->cmd_desc_phys,
					  GFP_KERNEL);
	if (!gpbc->ucarea) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		return -ENOMEM;
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */
#endif

	nand_gpbc_initHw2(gpbc);

	//the chip spec has been reflected in "options" on nand_gpbc_getInfoFromId.
	//So, clear the options not supported by the controller.
	gpbc->chip.options &= ~(NAND_CACHEPRG);

	gpbc->pagesPerBlock = mtd->erasesize / mtd->writesize;
	gpbc->sectSize3 = NAND_GPBC_ECCBYTES * (gpbc->mtd.writesize / NAND_GPBC_ECCSIZE - 1);
	gpbc->sectSize2 = NAND_GPBC_ECCSIZE - gpbc->sectSize3;
	gpbc->oobSkipSize = NAND_GPBC_OOB_SKIP_SIZE;

	gpbc->chip.bbt_td = &nand_gpbc_BbtMasterDesc;
	gpbc->chip.bbt_md = &nand_gpbc_BbtMirrorDesc;
#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	gpbc->chip.bbt_flag = &nand_gpbc_BbtFlagDesc;
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */
	gpbc->chip.badblock_pattern = &nand_gpbc_BbPatternDesc;

	gpbc->chip.ecc.mode = NAND_ECC_HW_SYNDROME;

	temp32 = NAND_GPBC_ECCBYTES * (mtd->writesize / NAND_GPBC_ECCSIZE);
	nand_gpbc_EccLayout.oobfree[0].offset = 4;
	nand_gpbc_EccLayout.oobfree[0].length = 4;
	nand_gpbc_EccLayout.oobfree[1].offset = NAND_GPBC_OOB_SKIP_SIZE + temp32;
	nand_gpbc_EccLayout.oobfree[1].length = mtd->oobsize - (NAND_GPBC_OOB_SKIP_SIZE + temp32);

	gpbc->chip.ecc.layout = &nand_gpbc_EccLayout;
	gpbc->chip.ecc.bytes = NAND_GPBC_ECCBYTES;
	gpbc->chip.ecc.strength = CONFIG_MTD_NAND_UNIPHIER_ECC_CORRECT_BITS;
	gpbc->chip.ecc.size = NAND_GPBC_ECCSIZE;
	gpbc->chip.ecc.prepad = 0;
	gpbc->chip.ecc.postpad = 0;

	gpbc->chip.ecc.hwctl = nand_gpbc_hwctl;
	gpbc->chip.ecc.calculate = nand_gpbc_calculate;
	gpbc->chip.ecc.correct = nand_gpbc_correct;
	gpbc->chip.ecc.read_page_raw = nand_gpbc_read_page_raw;
	gpbc->chip.ecc.write_page_raw = nand_gpbc_write_page_raw;
	gpbc->chip.ecc.read_page = nand_gpbc_read_page;
	gpbc->chip.ecc.read_subpage = nand_gpbc_read_subpage;
	gpbc->chip.ecc.write_page = nand_gpbc_write_page;
	gpbc->chip.ecc.write_oob_raw = nand_gpbc_write_oob;
	gpbc->chip.ecc.read_oob_raw = nand_gpbc_read_oob;
	gpbc->chip.ecc.read_oob = nand_gpbc_read_oob;
	gpbc->chip.ecc.write_oob = nand_gpbc_write_oob;

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	err = nand_gpbc_alloc_resources(gpbc);
	if (err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		goto out_err;
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

	err = nand_scan_tail(&gpbc->mtd);
	if (err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		goto out_err;
	}

	ppdata.of_node = dev->dev.of_node;
	err = mtd_device_parse_register(&gpbc->mtd, NULL, &ppdata, NULL, 0);
	if (err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		goto out_err;
	}

	err = nand_proc_init(gpbc);
	if(err) {
		pr_warn("\n## ERROR %s %s %d ##\n", __FILE__, __func__, __LINE__);
		goto out_err;
	}

	return 0;

out_err:
	nand_release(mtd);

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	nand_gpbc_free_resources(gpbc);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

#if defined(CONFIG_MTD_NAND_UNIPHIER_DMA) && defined(CONFIG_MTD_NAND_UNIPHIER_CDMA_READ)
	if (gpbc->cmd_desc) {
		dma_free_coherent(gpbc->dev, sizeof(struct nand_gpbc_cmd_desc_t),
				  gpbc->cmd_desc, gpbc->cmd_desc_phys);
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA && CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

#ifdef CONFIG_MTD_NAND_UNIPHIER_IRQ
	nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NGINTE);
#endif /* CONFIG_MTD_NAND_UNIPHIER_IRQ */

	return err;
}


static int nand_gpbc_remove(struct platform_device *dev)
{
	struct nand_gpbc_info *gpbc = platform_get_drvdata(dev);
	struct mtd_info		*mtd = &gpbc->mtd;

	nand_proc_exit();

	nand_release(mtd);

#ifdef CONFIG_MTD_NAND_UNIPHIER_BBM
	nand_gpbc_free_resources(gpbc);
#endif /* CONFIG_MTD_NAND_UNIPHIER_BBM */

#if defined(CONFIG_MTD_NAND_UNIPHIER_DMA) && defined(CONFIG_MTD_NAND_UNIPHIER_CDMA_READ)
	if (gpbc->cmd_desc) {
		dma_free_coherent(gpbc->dev, sizeof(struct nand_gpbc_cmd_desc_t),
				  gpbc->cmd_desc, gpbc->cmd_desc_phys);
	}
#endif /* CONFIG_MTD_NAND_UNIPHIER_DMA && CONFIG_MTD_NAND_UNIPHIER_CDMA_READ */

#ifdef CONFIG_MTD_NAND_UNIPHIER_IRQ
	nand_gpbc_write32(0, gpbc->regBase + NAND_GPBC_NGINTE);
#endif /* CONFIG_MTD_NAND_UNIPHIER_IRQ */

	return 0;
}

static const struct of_device_id nand_gpbc_ids[] = {
	{ .compatible = "socionext,uniphier-nand" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, nand_gpbc_ids);

static struct platform_driver nand_gpbc_driver = {
	.probe		= nand_gpbc_probe,
	.remove		= nand_gpbc_remove,
	.driver		= {
		.name	= NAND_GPBC_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table	= nand_gpbc_ids,
	},
};

module_platform_driver(nand_gpbc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Uniphier NAND controller driver");
