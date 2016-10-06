/*
 * spi-uniphier.c - Socionext UniPhier SCSSI/MCSSI controller driver
 *
 * Copyright 2012      Panasonic Corporation
 * Copyright 2016-2017 Socionext Inc.
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

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#define SPI_UNIPHIER_TIMEOUT	1000	/* ms */

#define DRIVER_NAME	"spi_uniphier"

#define SPI_UNIPHIER_TYPE_SCSSI	0
#define SPI_UNIPHIER_TYPE_MCSSI	1

struct spi_uniphier_pdata {
	unsigned long	tclk;
	unsigned int	device_type;
	unsigned int	num_chipselect;
};

struct spi_uniphier {
	struct device		*dev;
	void __iomem		*base;
	struct spi_master	*master;
	unsigned int		max_speed;
	unsigned int		min_speed;
	const struct spi_uniphier_pdata	*spi_pdata;
};

/* Register Offset */
#define SPI_UNIPHIER_CTL(cs)		(0x100 * (cs) + 0x00)
#define SPI_UNIPHIER_CKS(cs)		(0x100 * (cs) + 0x04)
#define SPI_UNIPHIER_TXWDS(cs)		(0x100 * (cs) + 0x08)
#define SPI_UNIPHIER_RXWDS(cs)		(0x100 * (cs) + 0x0c)
#define SPI_UNIPHIER_FPS(cs)		(0x100 * (cs) + 0x10)
#define SPI_UNIPHIER_SR(cs)		(0x100 * (cs) + 0x14)
#define SPI_UNIPHIER_IE(cs)		(0x100 * (cs) + 0x18)
#define SPI_UNIPHIER_IS(cs)		(0x100 * (cs) + 0x1c)
#define SPI_UNIPHIER_IC(cs)		(0x100 * (cs) + 0x1c)
#define SPI_UNIPHIER_FC(cs)		(0x100 * (cs) + 0x20)
#define SPI_UNIPHIER_TXDR(cs)		(0x100 * (cs) + 0x24)
#define SPI_UNIPHIER_RXDR(cs)		(0x100 * (cs) + 0x24)
#define SPI_UNIPHIER_EXR_MC(cs)		(0x100 * (cs) + 0x28)
#define SPI_UNIPHIER_TXDD_MC(cs)	(0x100 * (cs) + 0x2c)
#define SPI_UNIPHIER_RXCNT_MC(cs)	(0x100 * (cs) + 0x30)
#define SPI_UNIPHIER_NRXCNT_MC(cs)	(0x100 * (cs) + 0x34)
#define SPI_UNIPHIER_WTR_SC		0x3c
#define SPI_UNIPHIER_WTR_MC		0x1000

#define SPI_UNIPHIER_CTL_SSIEN		0x00000001
#define SPI_UNIPHIER_CTL_SSISEL_MC	0x00000002	/* only MCSSI */

#define SPI_UNIPHIER_CKS_CKRAT_MASK	0x000000FF

#define SPI_UNIPHIER_TXWDS_WDLEN_MASK	0x00003F00
#define SPI_UNIPHIER_TXWDS_DTLEN_MASK	0x0000003F

#define SPI_UNIPHIER_RXWDS_DTLEN_MASK	0x0000003F

#define SPI_UNIPHIER_FPS_FSPOL_MASK	0x00008000

#define SPI_UNIPHIER_SR_BUSY_SC		0x00000080	/* only SCSSI */
#define SPI_UNIPHIER_SR_TNF		0x00000020
#define SPI_UNIPHIER_SR_TFE		0x00000010
#define SPI_UNIPHIER_SR_RFF		0x00000002
#define SPI_UNIPHIER_SR_RNE		0x00000001

#define SPI_UNIPHIER_IE_TCIE_SC		0x00000010
#define SPI_UNIPHIER_IE_RCIE_SC		0x00000008
#define SPI_UNIPHIER_IE_TXIE_SC		0x00000004
#define SPI_UNIPHIER_IE_RXIE_SC		0x00000002
#define SPI_UNIPHIER_IE_RORIE_SC	0x00000001	/* only SCSSI */

#define SPI_UNIPHIER_IE_TCIE_MC		0x00000008
#define SPI_UNIPHIER_IE_RCIE_MC		0x00000004
#define SPI_UNIPHIER_IE_TXIE_MC		0x00000002
#define SPI_UNIPHIER_IE_RXIE_MC		0x00000001

#define SPI_UNIPHIER_IS_TCIS_SC		0x00001000
#define SPI_UNIPHIER_IS_RCIS_SC		0x00000800
#define SPI_UNIPHIER_IS_TXRS_SC		0x00000400
#define SPI_UNIPHIER_IS_RXRS_SC		0x00000200
#define SPI_UNIPHIER_IS_RORIS_SC	0x00000100	/* only SCSSI */
#define SPI_UNIPHIER_IS_TCID_SC		0x00000010
#define SPI_UNIPHIER_IS_RCID_SC		0x00000008
#define SPI_UNIPHIER_IS_TXRD_SC		0x00000004
#define SPI_UNIPHIER_IS_RXRD_SC		0x00000002
#define SPI_UNIPHIER_IS_RORID_SC	0x00000001	/* only SCSSI */

#define SPI_UNIPHIER_IS_TCIS_MC		0x00000800
#define SPI_UNIPHIER_IS_RCIS_MC		0x00000400
#define SPI_UNIPHIER_IS_TXRS_MC		0x00000200
#define SPI_UNIPHIER_IS_RXRS_MC		0x00000100
#define SPI_UNIPHIER_IS_TCID_MC		0x00000008
#define SPI_UNIPHIER_IS_RCID_MC		0x00000004
#define SPI_UNIPHIER_IS_TXRD_MC		0x00000002
#define SPI_UNIPHIER_IS_RXRD_MC		0x00000001

#define SPI_UNIPHIER_IC_TCIC_SC		0x00000010
#define SPI_UNIPHIER_IC_RCIC_SC		0x00000008
#define SPI_UNIPHIER_IC_RORIC_SC	0x00000001	/* only SCSSI */

#define SPI_UNIPHIER_IC_TCIC_MC		0x00000008
#define SPI_UNIPHIER_IC_RCIC_MC		0x00000004

#define SPI_UNIPHIER_FC_TXFFL		0x00001000
#define SPI_UNIPHIER_FC_TXFTH_MASK	0x00000F00
#define SPI_UNIPHIER_FC_RXFFL		0x00000010
#define SPI_UNIPHIER_FC_RXFTH_MASK	0x0000000F

static void spi_uniphier_writereg(struct spi_uniphier *spi_u,
				  unsigned int reg, u32 val)
{
	writel(val, spi_u->base + reg);
}

static u32 spi_uniphier_readreg(struct spi_uniphier *spi_u,
				unsigned int reg)
{
	return readl(spi_u->base + reg);
}

static int spi_uniphier_set_transfer_size(struct spi_device *spi, int size)
{
	struct spi_uniphier *spi_u;
	int cs = spi->chip_select;
	u32 temp;

	spi_u = spi_master_get_devdata(spi->master);

	temp = spi_uniphier_readreg(spi_u, SPI_UNIPHIER_TXWDS(cs));
	temp = (temp & ~SPI_UNIPHIER_TXWDS_WDLEN_MASK) | (size << 8);
	temp = (temp & ~SPI_UNIPHIER_TXWDS_DTLEN_MASK) | (size << 0);
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXWDS(cs), temp);

	temp = spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXWDS(cs));
	temp = (temp & ~SPI_UNIPHIER_RXWDS_DTLEN_MASK) | (size << 0);
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_RXWDS(cs), temp);

	/* reset FIFOs */
	/*      TXFFL       RXFFL */
	temp = (1 << 12) | (1 << 4);
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_FC(cs), temp);

	return 0;
}

static int spi_uniphier_set_baudrate(struct spi_device *spi, unsigned int speed)
{
	u32 tclk_hz;
	u32 ckrat;
	u32 temp;
	struct spi_uniphier *spi_u;
	int cs = spi->chip_select;

	spi_u = spi_master_get_devdata(spi->master);

	tclk_hz = spi_u->spi_pdata->tclk;

	/*
	 * the supported rates are: 4,6,8...254
	 * round up as we look for equal or less speed
	 */
	ckrat = DIV_ROUND_UP(tclk_hz, speed);
	ckrat = roundup(ckrat, 2);

	/* check if requested speed is too small */
	if (ckrat > 254)
		return -EINVAL;

	if (spi_u->spi_pdata->device_type == SPI_UNIPHIER_TYPE_MCSSI) {
		if (ckrat < 2)
			ckrat = 2;
	} else {
		if (ckrat < 4)
			ckrat = 4;
	}

	temp = spi_uniphier_readreg(spi_u, SPI_UNIPHIER_CKS(cs));
	temp = (temp & ~SPI_UNIPHIER_CKS_CKRAT_MASK) | ckrat;
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_CKS(cs), temp);

	return 0;
}

/*
 * called only when no transfer is active on the bus
 */
static int spi_uniphier_setup_transfer(struct spi_device *spi,
				       struct spi_transfer *t)
{
	struct spi_uniphier *spi_u;
	unsigned int speed = spi->max_speed_hz;
	unsigned int bits_per_word = spi->bits_per_word;
	int	rc;

	spi_u = spi_master_get_devdata(spi->master);

	if ((t != NULL) && t->speed_hz)
		speed = t->speed_hz;

	if ((t != NULL) && t->bits_per_word)
		bits_per_word = t->bits_per_word;

	rc = spi_uniphier_set_baudrate(spi, speed);
	if (rc)
		return rc;

	return spi_uniphier_set_transfer_size(spi, bits_per_word);
}

static void spi_uniphier_set_cs(struct spi_device *spi, int enable)
{
	struct spi_uniphier *spi_u;
	u32 fspol;
	u32 temp;
	int cs = spi->chip_select;

	spi_u = spi_master_get_devdata(spi->master);

	if (spi->mode & SPI_CS_HIGH)
		fspol = enable ? 1 : 0;
	else
		fspol = enable ? 0 : 1;

	temp = spi_uniphier_readreg(spi_u, SPI_UNIPHIER_FPS(cs));
	temp = (temp & ~SPI_UNIPHIER_FPS_FSPOL_MASK) | (fspol << 15);
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_FPS(cs), temp);
}

static int spi_uniphier_write_read_8bit(struct spi_device *spi,
					const u8 **tx_buf, u8 **rx_buf)
{
	struct spi_uniphier *spi_u;
	unsigned long timeout;
	int cs = spi->chip_select;

	spi_u = spi_master_get_devdata(spi->master);

	/* Wait for transimission FIFO is not full */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_TNF)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Write transmission data */
	if (tx_buf && *tx_buf)
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs),
			(unsigned long)*(*tx_buf)++);
	else
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs), 0);

	/* Wait for receive FIFO is not empty */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_RNE)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Read received data */
	if (rx_buf && *rx_buf)
		*(*rx_buf)++ = spi_uniphier_readreg(spi_u,
						    SPI_UNIPHIER_RXDR(cs));
	else
		spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXDR(cs));

	return 0;
}

static int spi_uniphier_write_read_16bit(struct spi_device *spi,
					 const u16 **tx_buf, u16 **rx_buf)
{
	struct spi_uniphier *spi_u;
	unsigned long timeout;
	int cs = spi->chip_select;

	spi_u = spi_master_get_devdata(spi->master);

	/* Wait for transimission FIFO is not full */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_TNF)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Write transmission data */
	if (tx_buf && *tx_buf)
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs),
			__cpu_to_le16(get_unaligned((*tx_buf)++)));
	else
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs), 0);

	/* Wait for receive FIFO is not empty */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_RNE)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Read received data */
	if (rx_buf && *rx_buf)
		*(*rx_buf)++ = spi_uniphier_readreg(spi_u,
						    SPI_UNIPHIER_RXDR(cs));
	else
		spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXDR(cs));

	return 0;

}

static int spi_uniphier_write_read_24bit(struct spi_device *spi,
					 const u8 **tx_buf, u8 **rx_buf)
{
	struct spi_uniphier *spi_u;
	unsigned long timeout;
	int cs = spi->chip_select;
	u32 temp;

	spi_u = spi_master_get_devdata(spi->master);

	/* Wait for transimission FIFO is not full */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_TNF)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Write transmission data */
	if (tx_buf && *tx_buf) {
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs),
			((*tx_buf)[2] << 16) | ((*tx_buf)[1] << 8) |
			 (*tx_buf)[0] << 0);
		*(u8 **)tx_buf += 3;
	} else {
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs), 0);
	}

	/* Wait for receive FIFO is not empty */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_RNE)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Read received data */
	if (rx_buf && *rx_buf) {
		temp = spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXDR(cs));
		(*rx_buf)[0] = (temp & 0x000000ff) >> 0;
		(*rx_buf)[1] = (temp & 0x0000ff00) >> 8;
		(*rx_buf)[2] = (temp & 0x00ff0000) >> 16;
		*(u8 **)rx_buf += 3;
	} else {
		spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXDR(cs));
	}

	return 0;

}

static int spi_uniphier_write_read_32bit(struct spi_device *spi,
					 const u32 **tx_buf, u32 **rx_buf)
{
	struct spi_uniphier *spi_u;
	unsigned long timeout;
	int cs = spi->chip_select;

	spi_u = spi_master_get_devdata(spi->master);

	/* Wait for transimission FIFO is not full */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_TNF)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	/* Write transmission data */
	if (tx_buf && *tx_buf) {
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs),
			__cpu_to_le32(get_unaligned((*tx_buf)++)));
	} else {
		spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXDR(cs), 0);
	}

	/* Wait for receive FIFO is not empty */
	timeout = jiffies + msecs_to_jiffies(SPI_UNIPHIER_TIMEOUT);
	while (!(spi_uniphier_readreg(spi_u, SPI_UNIPHIER_SR(cs)) &
		SPI_UNIPHIER_SR_RNE)) {
		if (time_after_eq(jiffies, timeout)) {
			dev_warn(&spi->dev, "%s, %d, %s: timeout\n",
				__FILE__, __LINE__, __func__);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	if (rx_buf && *rx_buf)
		*(*rx_buf)++ = spi_uniphier_readreg(spi_u,
						    SPI_UNIPHIER_RXDR(cs));
	else
		spi_uniphier_readreg(spi_u, SPI_UNIPHIER_RXDR(cs));

	return 0;

}

static unsigned int spi_uniphier_write_read(struct spi_device *spi,
					    struct spi_transfer *xfer)
{
	struct spi_uniphier *spi_u;
	unsigned int count;
	int word_len;
	int cs = spi->chip_select;
	u32 temp;

	spi_u = spi_master_get_devdata(spi->master);
	word_len = spi->bits_per_word;
	count = xfer->len;

	temp = SPI_UNIPHIER_CTL_SSIEN;
	if (spi->mode & SPI_LOOP)
		temp |= SPI_UNIPHIER_CTL_SSISEL_MC;

	/* Start transfer */
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_CTL(cs), temp);

	if (word_len <= 8) {
		const u8 *tx = xfer->tx_buf;
		u8 *rx = xfer->rx_buf;

		do {
			if (spi_uniphier_write_read_8bit(spi, &tx, &rx) < 0)
				goto out;
			count--;
		} while (count);
	} else if (word_len <= 16) {
		const u16 *tx = xfer->tx_buf;
		u16 *rx = xfer->rx_buf;

		do {
			if (spi_uniphier_write_read_16bit(spi, &tx, &rx) < 0)
				goto out;
			count -= 2;
		} while (count);
	} else if (word_len <= 24) {
		const u8 *tx = xfer->tx_buf;
		u8 *rx = xfer->rx_buf;

		do {
			if (spi_uniphier_write_read_24bit(spi, &tx, &rx) < 0)
				goto out;
			count -= 3;
		} while (count);
	} else {
		const u32 *tx = xfer->tx_buf;
		u32 *rx = xfer->rx_buf;

		do {
			if (spi_uniphier_write_read_32bit(spi, &tx, &rx) < 0)
				goto out;
			count -= 4;
		} while (count);
	}

	/* End transfer */
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_CTL(cs), 0);

out:
	return xfer->len - count;
}

static int spi_uniphier_transfer_one_message(struct spi_master *master,
					     struct spi_message *m)
{
	struct spi_uniphier *spi_u = spi_master_get_devdata(master);
	struct spi_device *spi = m->spi;
	struct spi_transfer *t = NULL;
	int par_override = 0;
	int status = 0;
	int cs_active = 0;

	/* Load defaults */
	status = spi_uniphier_setup_transfer(spi, NULL);

	if (status < 0)
		goto msg_done;

	list_for_each_entry(t, &m->transfers, transfer_list) {
		/* make sure buffer length is aligend with bit mode */
		if (((t->bits_per_word > 8) && (t->len & 1))
		    || ((t->bits_per_word > 16) && (t->len & 3))) {
			dev_err(&spi->dev,
				"message rejected : data length (%d) while in (%d) bit mode\n",
				t->len, t->bits_per_word);
			status = -EIO;
			goto msg_done;
		}

		/*
		 * allow only single-word transfer in none-multiples
		 * of 8-bit word
		 */
		if ((t->bits_per_word & 7)
		    && (t->len * 8 / t->bits_per_word != 1)) {
			dev_err(&spi->dev,
				"message rejected : multi-word transfer (%d) in none-multiple of 8-bit word (%d)\n",
				t->len, t->bits_per_word);
			status = -EIO;
			goto msg_done;
		}

		if (t->speed_hz && t->speed_hz < spi_u->min_speed) {
			dev_err(&spi->dev,
				"message rejected : device min speed (%d Hz) exceeds required transfer speed (%d Hz)\n",
				spi_u->min_speed, t->speed_hz);
			status = -EIO;
			goto msg_done;
		}

		if (par_override || t->speed_hz || t->bits_per_word) {
			par_override = 1;
			status = spi_uniphier_setup_transfer(spi, t);
			if (status < 0)
				break;
			if (!t->speed_hz && !t->bits_per_word)
				par_override = 0;
		}

		if (!cs_active) {
			spi_uniphier_set_cs(spi, 1);
			cs_active = 1;
		}

		if (t->len)
			m->actual_length += spi_uniphier_write_read(spi, t);

		if (t->delay_usecs)
			udelay(t->delay_usecs);

		if (t->cs_change) {
			spi_uniphier_set_cs(spi, 0);
			cs_active = 0;
		}
	}

msg_done:
	if (cs_active)
		spi_uniphier_set_cs(spi, 0);

	m->status = status;
	spi_finalize_current_message(master);

	return 0;
}

static int spi_uniphier_setup(struct spi_device *spi)
{
	struct spi_uniphier *spi_u;
	int cs = spi->chip_select;
	int mode = spi->mode;
	u32 temp;

	spi_u = spi_master_get_devdata(spi->master);

	if (((mode & (SPI_CPOL | SPI_CPHA)) == SPI_MODE_1) ||
	    ((mode & (SPI_CPOL | SPI_CPHA)) == SPI_MODE_2)) {
		dev_err(&spi->dev, "setup: unsupported SPI mode %d\n",
			mode & (SPI_CPOL | SPI_CPHA));
		return -EINVAL;
	}

	/* Setup SPI setting registers */
	if ((mode & (SPI_CPOL | SPI_CPHA)) == SPI_MODE_0)
		/*      CKPHS       CKINIT      CKDLY */
		temp = (1 << 14) | (0 << 13) | (1 << 12);
	else
		/*      CKPHS       CKINIT      CKDLY */
		temp = (1 << 14) | (1 << 13) | (0 << 12);

	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_CKS(cs), temp);

	if (mode & SPI_CS_HIGH)
		/*      FSPOL       FSTRT */
		temp = (0 << 15) | (0 << 14);
	else
		temp = (1 << 15) | (0 << 14);

	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_FPS(cs), temp);

	if (mode & SPI_LSB_FIRST)
		/*     TDTF/RDTF */
		temp = 1 << 6;
	else
		temp = 0 << 6;

	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_TXWDS(cs), temp);
	spi_uniphier_writereg(spi_u, SPI_UNIPHIER_RXWDS(cs), temp);

	if ((spi->max_speed_hz == 0)
			|| (spi->max_speed_hz > spi_u->max_speed))
		spi->max_speed_hz = spi_u->max_speed;

	if (spi->max_speed_hz < spi_u->min_speed) {
		dev_err(&spi->dev, "setup: requested speed too low %d Hz\n",
			spi->max_speed_hz);
		return -EINVAL;
	}

	/*
	 * baudrate & width will be set spi_uniphier_setup_transfer
	 */
	return 0;
}

static int spi_uniphier_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_master *master;
	struct spi_uniphier *spi_u;
	struct resource *res;
	const struct spi_uniphier_pdata *spi_pdata;
	resource_size_t len;
	int ret = 0;
	char name[] = "spi_uniphierX";

	spi_pdata = of_device_get_match_data(dev);
	if (WARN_ON(!spi_pdata))
		return -EINVAL;

	master = spi_alloc_master(dev, sizeof(*spi_u));
	if (master == NULL) {
		dev_err(dev, "failed to allocate master\n");
		return -ENOMEM;
	}

	if (pdev->id != -1)
		master->bus_num = pdev->id;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST;

	if (spi_pdata->device_type == SPI_UNIPHIER_TYPE_MCSSI)
		master->mode_bits |= SPI_LOOP;

	master->setup = spi_uniphier_setup;
	master->transfer_one_message = spi_uniphier_transfer_one_message;
	master->num_chipselect = spi_pdata->num_chipselect;
	master->bits_per_word_mask = ~0x7; /* SPI_BPW_RANGE_MASK(4, 32) */

	platform_set_drvdata(pdev, master);

	master->dev.of_node = pdev->dev.of_node;
	spi_u = spi_master_get_devdata(master);
	spi_u->dev = dev;
	spi_u->master = master;
	spi_u->spi_pdata = spi_pdata;

	if (spi_pdata->device_type == SPI_UNIPHIER_TYPE_MCSSI)
		spi_u->max_speed = DIV_ROUND_UP(spi_pdata->tclk, 2);
	else
		spi_u->max_speed = DIV_ROUND_UP(spi_pdata->tclk, 4);

	spi_u->min_speed = DIV_ROUND_UP(spi_pdata->tclk, 254);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto out_master_put;
	}
	len = resource_size(res);

	spi_u->base = devm_ioremap_resource(&pdev->dev, res);
	if (!spi_u->base) {
		ret = -ENOMEM;
		goto out_master_put;
	}

	dev_info(spi_u->dev, "type %s, base %#08lx\n",
		 (spi_pdata->device_type == SPI_UNIPHIER_TYPE_MCSSI) ?
		 "mcssi" : "scssi",
		 (unsigned long)spi_u->base);

	snprintf(name, sizeof(name), "spi_uniphier%d", pdev->id);

	ret = devm_spi_register_master(dev, master);
	if (ret) {
		dev_err(dev, "failed to register master (%d)\n", ret);
		goto out_master_put;
	}

	return ret;

out_master_put:
	spi_master_put(master);

	return ret;
}

static const struct spi_uniphier_pdata scssi_uniphier_pdata = {
	.tclk           = 50000000UL,
	.device_type    = SPI_UNIPHIER_TYPE_SCSSI,
	.num_chipselect = 1,
};
static const struct spi_uniphier_pdata mcssi_uniphier_pdata = {
	.tclk           = 10000000UL,
	.device_type    = SPI_UNIPHIER_TYPE_MCSSI,
	.num_chipselect = 4,
};

static const struct of_device_id uniphier_spi_match[] = {
	{
		.compatible = "socionext,uniphier-scssi",
		.data = &scssi_uniphier_pdata,
	},
	{
		.compatible = "socionext,uniphier-mcssi",
		.data = &mcssi_uniphier_pdata,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, uniphier_spi_match);

static struct platform_driver uniphier_spi_driver = {
	.probe = spi_uniphier_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(uniphier_spi_match),
	},
};
module_platform_driver(uniphier_spi_driver);

MODULE_DESCRIPTION("Socionext UniPhier SCSSI/MCSSI controller driver");
MODULE_LICENSE("GPL v2");
