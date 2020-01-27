/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
 *
 * Copyright (c) 2018 Socionext Inc.
 */

#ifndef DVB_UNIPHIER_ADAPTER_H__
#define DVB_UNIPHIER_ADAPTER_H__

struct uniphier_adapter_spec {
	int adapters;
	const struct hsc_conf *hsc_conf;
	const struct i2c_board_info *demod_i2c_info;
	const struct i2c_board_info *tuner_i2c_info;
};

struct uniphier_adapter_fe {
	struct i2c_client *demod_i2c;
	struct i2c_client *tuner_i2c;
	struct dvb_frontend *fe;
};

struct uniphier_adapter_priv {
	const struct uniphier_adapter_spec *spec;

	struct platform_device *pdev;
	struct hsc_chip *chip;

	struct platform_device *pdev_demux;
	struct clk *demod_mclk;
	struct gpio_desc *demod_gpio;
	struct i2c_adapter *demod_i2c_adapter;
	struct gpio_desc *tuner_gpio;
	struct i2c_adapter *tuner_i2c_adapter;

	struct uniphier_adapter_fe *fe;
};

struct i2c_client *dvb_module_probe(const char *module_name,
				    const char *name,
				    struct i2c_adapter *adap,
				    unsigned char addr,
				    void *platform_data);
void dvb_module_release(struct i2c_client *client);

int uniphier_adapter_demux_probe(struct uniphier_adapter_priv *priv);
void uniphier_adapter_demux_remove(struct uniphier_adapter_priv *priv);

#endif /* DVB_UNIPHIER_ADAPTER_H__ */
