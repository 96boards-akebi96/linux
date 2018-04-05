/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Socionext SC1501A series demodulator driver for ISDB-S/ISDB-T.
 *
 * Copyright (c) 2018 Socionext Inc.
 */

#ifndef SC1501A_H
#define SC1501A_H

#include <dvb_frontend.h>

/* ISDB-T IF frequency */
#define DIRECT_IF_57MHZ    57000000
#define DIRECT_IF_44MHZ    44000000
#define LOW_IF_4MHZ        4000000

struct sc1501a_config {
	struct clk *mclk;
	u32 if_freq;
	struct gpio_desc *reset_gpio;

	/* Everything after that is returned by the driver. */
	struct dvb_frontend **fe;
};

#endif /* SC1501A_H */
