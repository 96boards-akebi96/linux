// SPDX-License-Identifier: GPL-2.0
//
// Socionext UniPhier LD20 adapter driver for ISDB.
// Using Socionext MN884434 ISDB-S/ISDB-T demodulator and
// SONY HELENE tuner.
//
// Copyright (c) 2018 Socionext Inc.

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "hsc.h"
#include "uniphier-adapter.h"

int uniphier_adapter_demux_probe(struct uniphier_adapter_priv *priv)
{
	const struct uniphier_adapter_spec *spec = priv->spec;
	struct device *dev = &priv->pdev->dev;
	struct device_node *node;
	int ret, i;

	node = of_parse_phandle(dev->of_node, "demux", 0);
	if (!node) {
		dev_err(dev, "Failed to parse demux\n");
		return -ENODEV;
	}

	priv->pdev_demux = of_find_device_by_node(node);
	if (!priv->pdev_demux) {
		dev_err(dev, "Failed to find demux device\n");
		of_node_put(node);
		return -ENODEV;
	}
	of_node_put(node);

	priv->chip = platform_get_drvdata(priv->pdev_demux);

	for (i = 0; i < spec->adapters; i++) {
		ret = hsc_tsif_init(&priv->chip->tsif[i], &spec->hsc_conf[i]);
		if (ret) {
			dev_err(dev, "Failed to init TS I/F\n");
			return ret;
		}

		ret = hsc_dmaif_init(&priv->chip->dmaif[i], &spec->hsc_conf[i]);
		if (ret) {
			dev_err(dev, "Failed to init DMA I/F\n");
			return ret;
		}
	}

	return 0;
}

void uniphier_adapter_demux_remove(struct uniphier_adapter_priv *priv)
{
	int i;

	for (i = 0; i < priv->spec->adapters; i++) {
		hsc_dmaif_release(&priv->chip->dmaif[i]);
		hsc_tsif_release(&priv->chip->tsif[i]);
	}
}
