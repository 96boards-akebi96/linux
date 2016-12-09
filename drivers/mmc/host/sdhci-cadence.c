/*
 * Copyright (C) 2016 Socionext Inc.
 *   Author: Masahiro Yamada <yamada.masahiro@socionext.com>
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

#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#include "sdhci-pltfm.h"

/*
 * The tuned val register is 6 bit-wide, but not the whole of the range is
 * available.  Perhaps, 0 to 42 is available (43 is equivalent to 0), but
 * I am not quite sure if it is official.  Use only 0 to 39 for safety.
 */
#define SDHCI_CDNS_MAX_TUNING_LOOP	40

#define SDHCI_CDNS_IOADDR_OFFSET	0x200

#define SDHCI_CDNS_HRS04	    0x10
// delay regs address
  #define REG_DELAY_HS          0x00
  #define REG_DELAY_DEFAULT     0x01
  #define REG_DELAY_UHSI_SDR12  0x02
  #define REG_DELAY_UHSI_SDR25  0x03
  #define REG_DELAY_UHSI_SDR50  0x04
  #define REG_DELAY_UHSI_DDR50  0x05
  #define REG_DELAY_MMC_LEGACY  0x06
  #define REG_DELAY_MMC_SDR     0x07
  #define REG_DELAY_MMC_DDR     0x08

#define SDHCI_CDNS_HRS06		0x18
#define   SDHCI_CDNS_HRS06_TUNE_UP		BIT(15)
#define   SDHCI_CDNS_HRS06_TUNE_SHIFT		8
#define   SDHCI_CDNS_HRS06_TUNE_MASK		0x3f
#define   SDHCI_CDNS_HRS06_MODE_MASK		0x7
#define   SDHCI_CDNS_HRS06_MODE_SD		0x0
#define   SDHCI_CDNS_HRS06_MODE_MMC_SDR		0x2
#define   SDHCI_CDNS_HRS06_MODE_MMC_DDR		0x3
#define   SDHCI_CDNS_HRS06_MODE_MMC_HS200	0x4
#define   SDHCI_CDNS_HRS06_MODE_MMC_HS400	0x5

struct sdhci_cdns_priv {
	void __iomem *glue_addr;
};

static inline void *sdhci_cdns_priv(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return sdhci_pltfm_priv(pltfm_host);
}

static void sdhci_cdns_set_uhs_signaling(struct sdhci_host *host, unsigned timing)
{
	struct sdhci_cdns_priv *priv = sdhci_cdns_priv(host);
	u32 mode, tmp;

	switch (timing) {
	case MMC_TIMING_MMC_HS:
		mode = SDHCI_CDNS_HRS06_MODE_MMC_SDR;
		break;
	case MMC_TIMING_MMC_DDR52:
		mode = SDHCI_CDNS_HRS06_MODE_MMC_DDR;
		break;
	case MMC_TIMING_MMC_HS200:
		mode = SDHCI_CDNS_HRS06_MODE_MMC_HS200;
		break;
	case MMC_TIMING_MMC_HS400:
		mode = SDHCI_CDNS_HRS06_MODE_MMC_HS400;
		break;
	default:
		mode = SDHCI_CDNS_HRS06_MODE_SD;
		break;
	}

	tmp = readl(priv->glue_addr + SDHCI_CDNS_HRS06);
	tmp &= ~SDHCI_CDNS_HRS06_MODE_MASK;
	tmp |= mode;
	writel(tmp, priv->glue_addr + SDHCI_CDNS_HRS06);

	if (mode == SDHCI_CDNS_HRS06_MODE_SD)
		sdhci_set_uhs_signaling(host, timing);
}

static int sdhci_cdns_set_tune_val(struct sdhci_host *host, unsigned int val)
{
	struct sdhci_cdns_priv *priv = sdhci_cdns_priv(host);
	void __iomem *reg = priv->glue_addr + SDHCI_CDNS_HRS06;
	u32 tmp;

	if (WARN_ON(val > SDHCI_CDNS_HRS06_TUNE_MASK))
		return -EINVAL;

	tmp = readl(reg);
	tmp &= ~(SDHCI_CDNS_HRS06_TUNE_MASK << SDHCI_CDNS_HRS06_TUNE_SHIFT);
	tmp |= val << SDHCI_CDNS_HRS06_TUNE_SHIFT;
	tmp |= SDHCI_CDNS_HRS06_TUNE_UP;
	writel(tmp, reg);

	return readl_poll_timeout(reg, tmp, !(tmp & SDHCI_CDNS_HRS06_TUNE_UP),
				  0, 1);
}

static int sdhci_cdns_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	int cur_streak = 0;
	int max_streak = 0;
	int end_of_streak = 0;
	int i;

	if (host->timing != MMC_TIMING_MMC_HS200)
		return -ENOTSUPP;

	if (WARN_ON(opcode != MMC_SEND_TUNING_BLOCK_HS200))
		return -EINVAL;

	for (i = 0; i < SDHCI_CDNS_MAX_TUNING_LOOP; i++) {
		if (sdhci_cdns_set_tune_val(host, i) ||
		    mmc_send_tuning(host->mmc, opcode, NULL)) { /* bad */
			cur_streak = 0;
		} else { /* good */
			cur_streak++;
			if (cur_streak > max_streak) {
				max_streak = cur_streak;
				end_of_streak = i;
			}
		}
	}

	if (!max_streak) {
		dev_err(mmc_dev(host->mmc), "no tuning point found\n");
		return -EIO;
	}

	return sdhci_cdns_set_tune_val(host, end_of_streak - max_streak / 2);
}

static const struct sdhci_ops sdhci_cdns_ops = {
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.platform_execute_tuning = sdhci_cdns_execute_tuning,
	.set_uhs_signaling = sdhci_cdns_set_uhs_signaling,
};

static const struct sdhci_pltfm_data sdhci_cdns_pltfm_data = {
	.ops = &sdhci_cdns_ops,
	.quirks = SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static void sd4_set_dlyvr(void __iomem *ioaddr,
			  unsigned char addr, unsigned char data)
{
	u32 dlyrv_reg;

	dlyrv_reg = data << 8;
	dlyrv_reg |= addr;

	//set data and address
	writel(dlyrv_reg, ioaddr + SDHCI_CDNS_HRS04);
	dlyrv_reg |= (1 << 24);
	//send write request
	writel(dlyrv_reg, ioaddr + SDHCI_CDNS_HRS04);
	dlyrv_reg &= ~(1 << 24);
	//clear write request
	writel(dlyrv_reg, ioaddr + SDHCI_CDNS_HRS04);
}

static void sdhci_cdns_phy_init(void __iomem *ioaddr)
{
	sd4_set_dlyvr(ioaddr, REG_DELAY_DEFAULT, 0x04);
	sd4_set_dlyvr(ioaddr, REG_DELAY_HS, 0x0b);
	sd4_set_dlyvr(ioaddr, REG_DELAY_UHSI_SDR50, 0x06);
	sd4_set_dlyvr(ioaddr, REG_DELAY_UHSI_DDR50, 0x16);
	sd4_set_dlyvr(ioaddr, REG_DELAY_MMC_LEGACY, 9);
	sd4_set_dlyvr(ioaddr, REG_DELAY_MMC_SDR, 2);
	sd4_set_dlyvr(ioaddr, REG_DELAY_MMC_DDR, 3);
}

static int sdhci_cdns_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_cdns_priv *priv;
	int ret;

	host = sdhci_pltfm_init(pdev, &sdhci_cdns_pltfm_data, sizeof(*priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	priv = sdhci_cdns_priv(host);

	priv->glue_addr = host->ioaddr;
	host->ioaddr += SDHCI_CDNS_IOADDR_OFFSET;

	ret = mmc_of_parse(host->mmc);
	if (ret)
		goto free;

	sdhci_cdns_phy_init(priv->glue_addr);

	ret = sdhci_add_host(host);

free:
	if (ret)
		sdhci_pltfm_free(pdev);

	return ret;
}

static const struct of_device_id sdhci_cdns_match[] = {
	{ .compatible = "cdns,sd4hc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sdhci_cdns_match);

static struct platform_driver sdhci_cdns_driver = {
	.driver = {
		.name = "sdhci-cdns",
		.pm = &sdhci_pltfm_pmops,
		.of_match_table = sdhci_cdns_match,
	},
	.probe = sdhci_cdns_probe,
	.remove = sdhci_pltfm_unregister,
};
module_platform_driver(sdhci_cdns_driver);

MODULE_AUTHOR("Masahiro Yamada <yamada.masahiro@socionext.com>");
MODULE_DESCRIPTION("Cadence Secure Digital Host Controller");
MODULE_LICENSE("GPL");
