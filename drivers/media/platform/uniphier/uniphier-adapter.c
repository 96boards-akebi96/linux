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

/* copy from drivers/media/dvb-core/dvbdev.c */
struct i2c_client *dvb_module_probe(const char *module_name,
				    const char *name,
				    struct i2c_adapter *adap,
				    unsigned char addr,
				    void *platform_data)
{
	struct i2c_client *client;
	struct i2c_board_info *board_info;

	board_info = kzalloc(sizeof(*board_info), GFP_KERNEL);
	if (!board_info)
		return NULL;

	if (name)
		strlcpy(board_info->type, name, I2C_NAME_SIZE);
	else
		strlcpy(board_info->type, module_name, I2C_NAME_SIZE);

	board_info->addr = addr;
	board_info->platform_data = platform_data;
	request_module(module_name);
	client = i2c_new_device(adap, board_info);
	if (client == NULL || client->dev.driver == NULL) {
		kfree(board_info);
		return NULL;
	}

	if (!try_module_get(client->dev.driver->owner)) {
		i2c_unregister_device(client);
		client = NULL;
	}

	kfree(board_info);
	return client;
}

/* copy from drivers/media/dvb-core/dvbdev.c */
void dvb_module_release(struct i2c_client *client)
{
	if (!client)
		return;

	module_put(client->dev.driver->owner);
	i2c_unregister_device(client);
}

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
	if (!priv->chip) {
		dev_err(dev, "Failed to get demux data\n");
		return -ENODEV;
	}

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
