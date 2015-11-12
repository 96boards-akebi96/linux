/**
 * dwc3-uniphier.c - Socionext Uniphier DWC3 Specific Glue layer
 *
 * Copyright (c) 2015 Socionext Inc.
 *
 * Author:
 *	Kunihiko Hayashi <hayashi.kunihiko@socionext.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

/* for RESET_CTL */
#define XHCI_RESET_CTL_REG		(0x000)		/* Reset Control Register */
#define LINK_RESET_N			(1 << 15)
#define IOMMU_RESET_N			(1 << 14)

/* for VBUS_CONTROL */
#define XHCI_USB3_VBUS_CONTROL_REG	(0x100)		/* USB3 VBUS Control */
#define XHCI_USB3_VBUS_DRVVBUS_REG_EN	(1<<3)          /* VBUS ON Controled by EN */
#define XHCI_USB3_VBUS_DRVVBUS_REG	(1<<4)          /* VBUS ON */

/* for PXs2 SSPHY_TEST */
#define USB3_P_SSPHY_TEST_REG		(0x300)
#define TEST_I_OFFSET			0x00
#define TEST_O_OFFSET			0x04

/* for Pro5 {SSPHY,HSPHY}_PARAM */
#define XHCI_HSPHY_PARAM2_REG		(0x288)
#define XHCI_SSPHY_PARAM2_REG		(0x384)
#define XHCI_SSPHY_PARAM3_REG		(0x388)

/* for HOST_CONFIG0 */
#define HOST_CONFIG0_REG		(0x400)		/* SoC dependent register */
#define NUM_U3(n)			((n) << 11)
#define NUM_U2(n)			((n) <<  8)
#define NUM_U_MASK			(0x07)

#define UNIPHIER_USB_PORT_MAX 8

struct dwc3_uniphier {
	struct device		*dev;

	void __iomem		*base;
	struct clk		*u3clk[UNIPHIER_USB_PORT_MAX];
	struct clk		*u2clk[UNIPHIER_USB_PORT_MAX];
	bool			vbus_supply;
	u32			ss_instances;
	u32			hs_instances;
};

static inline void maskwritel(void __iomem *base, u32 offset, u32 mask, u32 value)
{
	void __iomem *addr = base + offset;

	writel((readl(addr) & ~(mask)) | (value & mask), addr);
}

/* for PXs2 */
static inline void pphy_test_io(void __iomem *vptr_i, void __iomem *vptr_o,
				u32 data0, u32 mask1, u32 data1)
{
	u32 tmp_data;
	u32 rd;
	u32 wd;
	u32 ud;
	u32 ld;
	u32 test_i;
	u32 test_o;

	ud = 1;
	ld = data0;
	tmp_data = ((ud & 0xff) << 6) | ((ld & 0x1f) << 1);
	wd = tmp_data;
	writel(wd, vptr_i);
	readl(vptr_o);
	readl(vptr_o);

	rd = readl(vptr_o);
	ud = (rd & ~mask1) | data1;
	ld = data0;
	tmp_data = ((ud & 0xff) << 6) | ((ld & 0x1f) << 1);
	wd = tmp_data;
	writel(wd, vptr_i);
	readl(vptr_o);
	readl(vptr_o);

	wd = tmp_data | 1;
	writel(wd, vptr_i);
	readl(vptr_o);
	readl(vptr_o);

	wd = tmp_data;
	writel(wd, vptr_i);
	readl(vptr_o);
	readl(vptr_o);

	ud = 1;
	ld = data0;
	tmp_data = ((ud & 0xff) << 6) | ((ld & 0x1f) << 1);
	rd = readl(vptr_i);
	wd = rd | tmp_data;
	writel(wd, vptr_i);
	readl(vptr_o);
	readl(vptr_o);

	test_i = readl(vptr_i);
	test_o = readl(vptr_o);
}

static void dwc3_uniphier_init_pxs2(struct dwc3_uniphier *dwc3u)
{
	int i;
	void __iomem *vptr, *vptr_i, *vptr_o;

	/* set numbers of port */
	maskwritel(dwc3u->base, HOST_CONFIG0_REG,
		   NUM_U3(NUM_U_MASK) | NUM_U2(NUM_U_MASK),
		   NUM_U3(dwc3u->ss_instances) | NUM_U2(dwc3u->hs_instances));

	/* control the VBUS  */
	if (!dwc3u->vbus_supply){
		maskwritel(dwc3u->base, XHCI_USB3_VBUS_CONTROL_REG,
			   XHCI_USB3_VBUS_DRVVBUS_REG | XHCI_USB3_VBUS_DRVVBUS_REG_EN,
			   XHCI_USB3_VBUS_DRVVBUS_REG_EN);
	}

	/* set up PHY */
	for(i=0; i < dwc3u->ss_instances; i++) {
		vptr = dwc3u->base + USB3_P_SSPHY_TEST_REG + (i * 0x10);
		vptr_i = vptr + TEST_I_OFFSET;
		vptr_o = vptr + TEST_O_OFFSET;
		pphy_test_io(vptr_i, vptr_o, 11, 0xf,  9);
		pphy_test_io(vptr_i, vptr_o,  9, 0xf,  3);
		pphy_test_io(vptr_i, vptr_o, 19, 0xf, 15);
		pphy_test_io(vptr_i, vptr_o,  8, 0xf,  3);
		pphy_test_io(vptr_i, vptr_o, 23, 0xc,  0);
		pphy_test_io(vptr_i, vptr_o, 28, 0x3,  1);
	}

	/* release reset */
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG, LINK_RESET_N, LINK_RESET_N );

	return;
}

static void dwc3_uniphier_init_pro5(struct dwc3_uniphier *dwc3u)
{
	/* set numbers of port */
	maskwritel(dwc3u->base, HOST_CONFIG0_REG,
		   NUM_U3(NUM_U_MASK) | NUM_U2(NUM_U_MASK),
		   NUM_U3(dwc3u->ss_instances) | NUM_U2(dwc3u->hs_instances));

	/* control the VBUS  */
	if (!dwc3u->vbus_supply){
		maskwritel(dwc3u->base, XHCI_USB3_VBUS_CONTROL_REG,
			   XHCI_USB3_VBUS_DRVVBUS_REG | XHCI_USB3_VBUS_DRVVBUS_REG_EN,
			   XHCI_USB3_VBUS_DRVVBUS_REG_EN);
	}

	/* set up PHY */
	/* SSPHY Reference Clock Enable for SS function */
	maskwritel(dwc3u->base, XHCI_SSPHY_PARAM3_REG, 0x80000000, 0x80000000);
	/* HSPHY MPLL Frequency Multiplier Control, Frequency Select */
	maskwritel(dwc3u->base, XHCI_HSPHY_PARAM2_REG, 0x7f7f0000, 0x7d310000);
	/* SSPHY Loopback off */
	maskwritel(dwc3u->base, XHCI_SSPHY_PARAM2_REG, 0x00800000, 0x00000000);
	/* release PHY power on reset */
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG, 0x00030000, 0x00000000);

	/* release reset */
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG,
		   LINK_RESET_N | IOMMU_RESET_N, 0x00000000);
	msleep(1);
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG,
		   LINK_RESET_N | IOMMU_RESET_N,
		   LINK_RESET_N | IOMMU_RESET_N );

	return;
}

static void dwc3_uniphier_exit_pxs2(struct dwc3_uniphier *dwc3u)
{
	/* reset */
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG, LINK_RESET_N, 0x00000000);

	/* control the VBUS */
	if (!dwc3u->vbus_supply){
		maskwritel(dwc3u->base, XHCI_USB3_VBUS_CONTROL_REG,
			   XHCI_USB3_VBUS_DRVVBUS_REG_EN, 0x00000000);
	}

	return;
}

static void dwc3_uniphier_exit_pro5(struct dwc3_uniphier *dwc3u)
{
	/* reset */
	maskwritel(dwc3u->base, XHCI_RESET_CTL_REG,
		   LINK_RESET_N | IOMMU_RESET_N, 0x00000000);

	/* control the VBUS */
	if (!dwc3u->vbus_supply){
		maskwritel(dwc3u->base, XHCI_USB3_VBUS_CONTROL_REG,
			   XHCI_USB3_VBUS_DRVVBUS_REG_EN, 0x00000000);
	}

	return;
}

static int dwc3_uniphier_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct dwc3_uniphier *dwc3u;
	struct resource	*res;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int ret;
	int i, u3count, u2count;
	char clkname[8];

	dwc3u = devm_kzalloc(&pdev->dev, sizeof(*dwc3u), GFP_KERNEL);
	if (!dwc3u)
		return -ENOMEM;

	platform_set_drvdata(pdev, dwc3u);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	dwc3u->dev         = dev;
	dwc3u->base        = base;
	dwc3u->vbus_supply = of_property_read_bool(node, "vbus-supply");

	/* SS(U3) instances */
	if ((of_property_read_u32(node, "ss-instances", &dwc3u->ss_instances))
	  || (dwc3u->ss_instances > NUM_U_MASK)) {
		dev_err(dev, "illegal SS instances (%d)\n", dwc3u->ss_instances);
		return -EINVAL;
	}

	/* HS(U2) instances */
	if ((of_property_read_u32(node, "hs-instances", &dwc3u->hs_instances))
	  || (dwc3u->hs_instances > NUM_U_MASK)) {
		dev_err(dev, "illegal HS instances (%d)\n", dwc3u->hs_instances);
		return -EINVAL;
	}

	/* clk control */
	u3count = 0;
	u2count = 0;
	for(i=0; i<UNIPHIER_USB_PORT_MAX; i++) {

		/* usb3 clock */
		sprintf(clkname, "u3clk%d", i);
		dwc3u->u3clk[i] = devm_clk_get(dwc3u->dev, clkname);
		if (dwc3u->u3clk[i] == ERR_PTR(-ENOENT)) {
			dwc3u->u3clk[i] = NULL;
		}
		else if (IS_ERR(dwc3u->u3clk[i])) {
			dev_err(dwc3u->dev, "failed to get usb3 clock%d\n",i);
			return PTR_ERR(dwc3u->u3clk[i]);
		}
		else {
			u3count++;
		}

		/* usb2 clock */
		sprintf(clkname, "u2clk%d", i);
		dwc3u->u2clk[i] = devm_clk_get(dwc3u->dev, clkname);
		if (dwc3u->u2clk[i] == ERR_PTR(-ENOENT)) {
			dwc3u->u2clk[i] = NULL;
		}
		else if (IS_ERR(dwc3u->u2clk[i])) {
			dev_err(dwc3u->dev, "failed to get usb2 clock%d\n",i);
			return PTR_ERR(dwc3u->u2clk[i]);
		}
		else {
			u2count++;
		}
	}
	if ((u2count == 0)&&(u3count == 0)) {
		dev_err(dwc3u->dev, "failed to get usb clock\n");
		return -ENOENT;
	}

	/* enable clk */
	for(i=0;i<UNIPHIER_USB_PORT_MAX;i++) {
		if (dwc3u->u3clk[i]) {
			ret = clk_prepare_enable(dwc3u->u3clk[i]);
			if (ret) {
				dev_err(dwc3u->dev, "failed to enable usb3 clock\n");
				goto err_clks;
			}
		}
		if (dwc3u->u2clk[i]) {
			ret = clk_prepare_enable(dwc3u->u2clk[i]);
			if (ret) {
				dev_err(dwc3u->dev, "failed to enable usb2 clock\n");
				goto err_clks;
			}
		}
	}

	/* initialize SoC glue */
	if (of_device_is_compatible(node, "socionext,proxstream2-dwc3")) {
		dwc3_uniphier_init_pxs2(dwc3u);
	}
	else if (of_device_is_compatible(node, "socionext,ph1-pro5-dwc3")) {
		dwc3_uniphier_init_pro5(dwc3u);
	}

	ret = of_platform_populate(node, NULL, NULL, dwc3u->dev);
	if (ret) {
		dev_err(dwc3u->dev, "failed to register core - %d\n", ret);
		goto err_dwc3;
	}

	return 0;

err_dwc3:
	if (of_device_is_compatible(node, "socionext,proxstream2-dwc3")) {
		dwc3_uniphier_exit_pxs2(dwc3u);
	}
	else if (of_device_is_compatible(node, "socionext,ph1-pro5-dwc3")) {
		dwc3_uniphier_exit_pro5(dwc3u);
	}

err_clks:
	/* disable clk */
	for(i=0;i<UNIPHIER_USB_PORT_MAX;i++) {
		if (dwc3u->u3clk[i])
			clk_disable_unprepare(dwc3u->u3clk[i]);
		if (dwc3u->u2clk[i])
			clk_disable_unprepare(dwc3u->u2clk[i]);
	}

	return ret;
}

static int dwc3_uniphier_remove(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct dwc3_uniphier *dwc3u = platform_get_drvdata(pdev);
	int i;

	of_platform_depopulate(&pdev->dev);

	if (of_device_is_compatible(node, "socionext,proxstream2-dwc3")) {
		dwc3_uniphier_exit_pxs2(dwc3u);
	}
	else if (of_device_is_compatible(node, "socionext,ph1-pro5-dwc3")) {
		dwc3_uniphier_exit_pro5(dwc3u);
	}

	for(i=0;i<UNIPHIER_USB_PORT_MAX;i++) {
		if (dwc3u->u3clk[i]) {
			clk_disable_unprepare(dwc3u->u3clk[i]);
			devm_clk_put(dwc3u->dev, dwc3u->u3clk[i]);
		}
		if (dwc3u->u2clk[i]) {
			clk_disable_unprepare(dwc3u->u2clk[i]);
			devm_clk_put(dwc3u->dev, dwc3u->u2clk[i]);
		}
	}
	return 0;
}

static const struct of_device_id of_dwc3_match[] = {
	{ .compatible = "socionext,proxstream2-dwc3" },
	{ .compatible = "socionext,ph1-pro5-dwc3" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_dwc3_match);

static struct platform_driver dwc3_uniphier_driver = {
	.probe		= dwc3_uniphier_probe,
	.remove		= dwc3_uniphier_remove,
	.driver		= {
		.name	= "uniphier-dwc3",
		.of_match_table	= of_dwc3_match,
	},
};

module_platform_driver(dwc3_uniphier_driver);

MODULE_ALIAS("platform:uniphier-dwc3");
MODULE_AUTHOR("Kunihiko Hayashi <hayashi.kunihiko@socionext.com>");
MODULE_DESCRIPTION("DesignWare USB3 UniPhier Glue Layer");
MODULE_LICENSE("GPL v2");
