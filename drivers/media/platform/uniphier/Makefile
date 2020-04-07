# SPDX-License-Identifier: GPL-2.0
uniphier-dvb-y += hsc-dma.o hsc-css.o hsc-ts.o hsc-ucode.o hsc-core.o
uniphier-dvb-$(CONFIG_DVB_UNIPHIER_LD11) += hsc-ld11.o

obj-$(CONFIG_DVB_UNIPHIER) += uniphier-dvb.o

ccflags-y += -I$(srctree)/drivers/media/dvb-frontends/
ccflags-y += -I$(srctree)/drivers/media/tuners/

uniphier-ld11-mn884433-helene-y += uniphier-adapter.o ld11-mn884433-helene.o
obj-$(CONFIG_DVB_UNIPHIER_LD11_ISDB) += uniphier-ld11-mn884433-helene.o
