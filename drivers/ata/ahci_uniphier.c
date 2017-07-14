/*
 * drivers/ata/ahci_uniphier.c
 *
 * Copyright (c) 2016-2017, Socionext Inc.
 * All rights reserved.
 *
 * Author:
 *	Kunihiko Hayashi <hayashi.kunihiko@socionext.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/ahci_platform.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#include "ahci.h"

#define DRV_NAME "uniphier-ahci"

struct uniphier_ahci_soc_data {
	int (*init)(struct ahci_host_priv *);
};

struct uniphier_ahci_priv {
	struct device *dev;
	void __iomem  *sata_regs;
	struct reset_control   *rst_link;
	struct reset_control   *rst_phy;
	struct uniphier_ahci_soc_data *data;
};

/* Glue registers */
#define SATA_RSTCTRL				(0x0000)
#define SATA_RSTCTRL_XRSTMMU			BIT(4)
#define SATA_RSTCTRL_XRSTLINK			BIT(0)
#define SATA_RSTCTRL_XRSTLINK_MASK		GENMASK(3, 0)

#define SATA_AXIUSER				(0x0004)

#define SATA_CKCTRL0				(0x0010)
#define SATA_CKCTRL0_MPLL_SSC_EN		BIT(17)
#define SATA_CKCTRL0_MPLL_CK_OFF		BIT(9)
#define SATA_CKCTRL0_MPLL_NCY_MASK		GENMASK(8, 4)
#define SATA_CKCTRL0_MPLL_NCY_SHIFT		4
#define SATA_CKCTRL0_MPLL_NCY5_MASK		GENMASK(3, 2)
#define SATA_CKCTRL0_MPLL_NCY5_SHIFT		2
#define SATA_CKCTRL0_MPLL_PRESCALE_MASK		GENMASK(1, 0)
#define SATA_CKCTRL0_MPLL_PRESCALE_SHIFT	0

#define SATA_CKCTRL1				(0x0014)
#define SATA_CKCTRL1_LOS_LVL_MASK		GENMASK(20, 16)
#define SATA_CKCTRL1_LOS_LVL_SHIFT		16
#define SATA_CKCTRL1_TX_LVL_MASK		GENMASK(12, 8)
#define SATA_CKCTRL1_TX_LVL_SHIFT		8

#define SATA_CKCTRL				(0x0010)
#define SATA_CKCTRL_REF_SSP_EN			BIT(9)
#define SATA_CKCTRL_P0_RESET			BIT(10)
#define SATA_CKCTRL_P0_READY			BIT(15)

#define SATA_RXTXCTRL				(0x0018)
#define SATA_RXTXCTRL_RX_EQ_VALL_MASK		GENMASK(31, 29)
#define SATA_RXTXCTRL_RX_EQ_VALL_SHIFT		29
#define SATA_RXTXCTRL_RX_DPLL_MODE_MASK		GENMASK(28, 26)
#define SATA_RXTXCTRL_RX_DPLL_MODE_SHIFT	26
#define SATA_RXTXCTRL_TX_ATTEN_MASK		GENMASK(14, 12)
#define SATA_RXTXCTRL_TX_ATTEN_SHIFT		12
#define SATA_RXTXCTRL_TX_BOOST_MASK		GENMASK(11, 8)
#define SATA_RXTXCTRL_TX_BOOST_SHIFT		8
#define SATA_RXTXCTRL_TX_EDGERATE_MASK		GENMASK(3, 2)
#define SATA_RXTXCTRL_TX_EDGERATE_SHIFT		2
#define SATA_RXTXCTRL_TX_CKO_EN			BIT(0)

#define SATA_RSTPWR				(0x0040)
#define SATA_RSTPWR_RX_EN_VAL			BIT(18)

/* AHCI registers */
#define AHCI_OOBR				(0x00bc)
#define AHCI_OOBR_WE				BIT(31)
#define AHCI_OOBR_CWMIN_SHIFT			24
#define AHCI_OOBR_CWMAX_SHIFT			16
#define AHCI_OOBR_CIMIN_SHIFT			8
#define AHCI_OOBR_CIMAX_SHIFT			0

/* SC registers */
#define SC_REG_BASE				(0x61840000)
#define SC_REG_SIZE				(0x10000)
#define SC_SATASRCSEL				(0x2210)
#define SC_SATASRCSEL_EN			BIT(4)
#define SC_SATASRCSEL_MASK			GENMASK(3, 0)
#define SC_SATASRCSEL_PLL	   		0x0
#define SC_SATASRCSEL_AXO_SINGLE   		0xc

/* SG registers */
#define SG_REG_BASE				(0x5f800000)
#define SG_REG_SIZE				(0x2000)
#define SG_SAPREFCLKSEL				(0x1a28)
#define SG_SAPREFCLKSEL_MASK			BIT(0)
#define SG_SAPREFCLKSEL_PLL			0x0
#define SG_SAPREFCLKSEL_AXO			0x1

#define AHCI_PHY_TIMEOUT 200

static void maskwritel(void __iomem *base, u32 offset, u32 mask, u32 value)
{
	void __iomem *addr = base + offset;

	writel((readl(addr) & ~(mask)) | (value & mask), addr);
}

static void uniphier_ahci_configure_oob(struct ahci_host_priv *hpriv)
{
	u32 old_val, new_val;

	new_val = (0x04 << AHCI_OOBR_CWMIN_SHIFT) |
		  (0x08 << AHCI_OOBR_CWMAX_SHIFT) |
		  (0x0e << AHCI_OOBR_CIMIN_SHIFT) |
		  (0x1a << AHCI_OOBR_CIMAX_SHIFT);

	old_val = readl(hpriv->mmio + AHCI_OOBR);
	writel(old_val | AHCI_OOBR_WE, hpriv->mmio + AHCI_OOBR);
	writel(new_val | AHCI_OOBR_WE, hpriv->mmio + AHCI_OOBR);
	writel(new_val, hpriv->mmio + AHCI_OOBR);
}

static int uniphier_ahci_phy_init_pro4(struct ahci_host_priv *hpriv)
{
	struct uniphier_ahci_priv *upriv = hpriv->plat_data;
	void __iomem *vptr;
	int timeout = AHCI_PHY_TIMEOUT;
	u32 val;

	/* negate LINK core reset */
#if defined(CONFIG_RESET_UNIPHIER)
	reset_control_deassert(upriv->rst_link);
#endif /* CONFIG_RESET_UNIPHIER */

	/* FIXME: not supported sata clock selector (bit0=0 GPLL(62.5MHz) -> bit0=1 AXO(25MHz)) */
	vptr = ioremap_nocache(SG_REG_BASE, SG_REG_SIZE);
	maskwritel(vptr, SG_SAPREFCLKSEL,
		   SG_SAPREFCLKSEL_MASK, SG_SAPREFCLKSEL_AXO);
	iounmap(vptr);

	/* set PHY reference clock frequency */
	maskwritel(upriv->sata_regs, SATA_CKCTRL0,
		   SATA_CKCTRL0_MPLL_NCY_MASK |
		   SATA_CKCTRL0_MPLL_NCY5_MASK |
		   SATA_CKCTRL0_MPLL_PRESCALE_MASK,
		   (0x6 << SATA_CKCTRL0_MPLL_NCY_SHIFT) |
		   (0x2 << SATA_CKCTRL0_MPLL_NCY5_SHIFT) |
		   (0x1 << SATA_CKCTRL0_MPLL_PRESCALE_SHIFT));

	/* set up TX/RX for Gen2 */
	maskwritel(upriv->sata_regs, SATA_CKCTRL1,
		   SATA_CKCTRL1_LOS_LVL_MASK |
		   SATA_CKCTRL1_TX_LVL_MASK,
		   (0x10 << SATA_CKCTRL1_LOS_LVL_SHIFT) |
		   (0x06 << SATA_CKCTRL1_TX_LVL_SHIFT));

	maskwritel(upriv->sata_regs, SATA_RXTXCTRL,
		   SATA_RXTXCTRL_RX_EQ_VALL_MASK |
		   SATA_RXTXCTRL_RX_DPLL_MODE_MASK |
		   SATA_RXTXCTRL_TX_ATTEN_MASK |
		   SATA_RXTXCTRL_TX_BOOST_MASK |
		   SATA_RXTXCTRL_TX_EDGERATE_MASK,
		   (0x6 << SATA_RXTXCTRL_RX_EQ_VALL_SHIFT) |
		   (0x3 << SATA_RXTXCTRL_RX_DPLL_MODE_SHIFT) |
		   (0x3 << SATA_RXTXCTRL_TX_ATTEN_SHIFT) |
		   (0x5 << SATA_RXTXCTRL_TX_BOOST_SHIFT) |
		   (0x0 << SATA_RXTXCTRL_TX_EDGERATE_SHIFT));

	/* enable spread spectrum */
	maskwritel(upriv->sata_regs, SATA_CKCTRL0,
		   SATA_CKCTRL0_MPLL_SSC_EN,
		   SATA_CKCTRL0_MPLL_SSC_EN);

	/* turn reference clock on */
	maskwritel(upriv->sata_regs, SATA_CKCTRL0,
		   SATA_CKCTRL0_MPLL_CK_OFF, 0);

	/* enable TX clock */
	maskwritel(upriv->sata_regs, SATA_RXTXCTRL,
		   SATA_RXTXCTRL_TX_CKO_EN,
		   SATA_RXTXCTRL_TX_CKO_EN);

	/* release PHY reset */
#if defined(CONFIG_RESET_UNIPHIER)
	reset_control_deassert(upriv->rst_phy);
#endif /* CONFIG_RESET_UNIPHIER */

	/* negate LINK local reset */
	maskwritel(upriv->sata_regs, SATA_RSTCTRL,
		   SATA_RSTCTRL_XRSTLINK,
		   SATA_RSTCTRL_XRSTLINK);

	/* wait until RX is ready */
	do {
		val = readl(upriv->sata_regs + SATA_RSTPWR);
	} while ((!(val & SATA_RSTPWR_RX_EN_VAL)) && (--timeout)) ;

	/*
	 * RSTPWR.RX_EN_VAL is asserted when negating LINK reset of ALL ports.
	 * Therefore, checking RX for port0 always fails when SATA has 2 port.
	 */
	if (!timeout)
		dev_warn(upriv->dev, "failed to check whether RX is ready\n");

	/* release all LINK reset */
	maskwritel(upriv->sata_regs, SATA_RSTCTRL,
		   SATA_RSTCTRL_XRSTLINK_MASK | SATA_RSTCTRL_XRSTMMU,
		   SATA_RSTCTRL_XRSTLINK_MASK | SATA_RSTCTRL_XRSTMMU);

	/* set OOB control register */
	uniphier_ahci_configure_oob(hpriv);

	return 0;
}

static int uniphier_ahci_phy_init_pxs2(struct ahci_host_priv *hpriv)
{
	struct uniphier_ahci_priv *upriv = hpriv->plat_data;
	void __iomem *vptr;
	u32 timeout = AHCI_PHY_TIMEOUT;
	u32 val;

	/* FIXME: not supported sata clock selector (bit4=1,bit[3:0]=0xc) */
	vptr = ioremap_nocache(SC_REG_BASE, SC_REG_SIZE);
	if (!vptr) {
		dev_err(upriv->dev, "failed to map clock selector\n");
		return -ENODEV;
	}
	maskwritel(vptr, SC_SATASRCSEL,
		   SC_SATASRCSEL_EN | SC_SATASRCSEL_MASK,
		   SC_SATASRCSEL_EN | SC_SATASRCSEL_AXO_SINGLE);
	iounmap(vptr);

	/* negate LINK core reset */
#if defined(CONFIG_RESET_UNIPHIER)
	reset_control_deassert(upriv->rst_link);
#endif /* CONFIG_RESET_UNIPHIER */

	/* enable reference clock for PHY */
	maskwritel(upriv->sata_regs, SATA_CKCTRL,
		   SATA_CKCTRL_REF_SSP_EN, SATA_CKCTRL_REF_SSP_EN);

	/* negate PHY port reset */
	maskwritel(upriv->sata_regs, SATA_CKCTRL,
		   SATA_CKCTRL_P0_RESET, 0);

	/* negate PHY core reset */
#if defined(CONFIG_RESET_UNIPHIER)
	reset_control_deassert(upriv->rst_phy);
#endif /* CONFIG_RESET_UNIPHIER */

	/* release LINK reset */
	maskwritel(upriv->sata_regs, SATA_RSTCTRL,
		   SATA_RSTCTRL_XRSTLINK, SATA_RSTCTRL_XRSTLINK);

	/* wait until PHY PLL is ready */
	do {
		val = readl(upriv->sata_regs + SATA_CKCTRL);
		msleep(20);
	} while ((!(val & SATA_CKCTRL_P0_READY)) && (--timeout));
	if (!timeout) {
		dev_err(upriv->dev, "failed to reset PHY\n");
		return -EIO;
	}

	return 0;
}

static void uniphier_ahci_host_stop(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;

	ahci_platform_disable_resources(hpriv);
}

static struct ata_port_operations uniphier_ahci_port_ops = {
	.inherits	= &ahci_platform_ops,
	.host_stop	= uniphier_ahci_host_stop,
};

static const struct ata_port_info uniphier_ahci_port_info = {
	.flags          = AHCI_FLAG_COMMON,
	.pio_mask       = ATA_PIO4,
	.udma_mask      = ATA_UDMA6,
	.port_ops       = &uniphier_ahci_port_ops,
};

static struct scsi_host_template ahci_platform_sht = {
	AHCI_SHT(DRV_NAME),
};


static const struct of_device_id uniphier_ahci_match[];

static int uniphier_ahci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv;
	struct uniphier_ahci_priv *upriv;
	struct resource *res;
	const struct of_device_id *of_id;
	int ret;

	of_id = of_match_device(uniphier_ahci_match, dev);
	if (!of_id)
		return -EINVAL;

	upriv = devm_kzalloc(dev, sizeof(*upriv), GFP_KERNEL);
	if (!upriv)
		return -ENOMEM;
	upriv->dev = dev;

#if defined(CONFIG_RESET_UNIPHIER)
	upriv->rst_link = devm_reset_control_get_shared(dev, "sata-link");
	if (IS_ERR(upriv->rst_link))
		return PTR_ERR(upriv->rst_link);

	upriv->rst_phy = devm_reset_control_get_shared(dev, "sata-phy");
	if (IS_ERR(upriv->rst_phy))
		return PTR_ERR(upriv->rst_phy);
#endif /* CONFIG_RESET_UNIPHIER */

	upriv->data = (struct uniphier_ahci_soc_data *)of_id->data;

	hpriv = ahci_platform_get_resources(pdev);
	if (IS_ERR(hpriv))
		return PTR_ERR(hpriv);

	hpriv->plat_data = upriv;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	upriv->sata_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(upriv->sata_regs))
		return PTR_ERR(upriv->sata_regs);

	ret = ahci_platform_enable_resources(hpriv);
	if (ret)
		return ret;

	if (upriv->data->init) {
		ret = (upriv->data->init)(hpriv);
		if (ret)
			goto err_res;
	}

	/* force to set HOSTS_PORTS_IMPL (for AHCI 1.3) */
	writel(BIT(0), hpriv->mmio + HOST_PORTS_IMPL);

	ret = ahci_platform_init_host(pdev, hpriv, &uniphier_ahci_port_info,
				      &ahci_platform_sht);
	if (ret)
		goto err_res;

	return 0;

err_res:
	ahci_platform_disable_resources(hpriv);
	return ret;
}

static const struct uniphier_ahci_soc_data uniphier_ahci_soc_data_pro4 = {
	.init = uniphier_ahci_phy_init_pro4,
};
static const struct uniphier_ahci_soc_data uniphier_ahci_soc_data_pxs2 = {
	.init = uniphier_ahci_phy_init_pxs2,
};

static const struct of_device_id uniphier_ahci_match[] = {
	{
		.compatible = "socionext,uniphier-pro4-ahci",
		.data = &uniphier_ahci_soc_data_pro4,
	},
	{
		.compatible = "socionext,uniphier-pxs2-ahci",
		.data = &uniphier_ahci_soc_data_pxs2,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, uniphier_ahci_match);

static struct platform_driver uniphier_ahci_driver = {
	.probe = uniphier_ahci_probe,
	.remove = ata_platform_remove_one,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(uniphier_ahci_match),
	},
};
module_platform_driver(uniphier_ahci_driver);


MODULE_AUTHOR("Kunihiko Hayashi <hayashi.kunihiko@socionext.com>");
MODULE_DESCRIPTION("UniPhier AHCI SATA driver");
MODULE_LICENSE("GPL v2");
