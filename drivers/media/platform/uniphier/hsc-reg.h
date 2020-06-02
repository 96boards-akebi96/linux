/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Socionext UniPhier DVB driver for High-speed Stream Controller (HSC).
 *
 * Copyright (c) 2018 Socionext Inc.
 */

#ifndef DVB_UNIPHIER_HSC_REG_H__
#define DVB_UNIPHIER_HSC_REG_H__

/* RAM Address */
#define FLT_PATN_RAM_TOP_ADDR           0x0a000
#define FLT_MASK_RAM_TOP_ADDR           0x0b000
#define SHARE_MEMORY_0_NORMAL           0x10000
#define SHARE_MEMORY_1_NORMAL           0x11000
#define SHARE_MEMORY_2_NORMAL           0x12000
#define SHARE_MEMORY_3_NORMAL           0x13000
#define SHARE_MEMORY_4_NORMAL           0x14000
#define SHARE_MEMORY_5_NORMAL           0x15000
#define SHARE_MEMORY_6_NORMAL           0x16000
#define SHARE_MEMORY_7_NORMAL           0x17000

/* RAM size */
#define FLT_PATN_RAM_SIZE               0x0800
#define FLT_MASK_RAM_SIZE               0x0800
#define FLT_PIDPATTERN_SIZE             0x0160
#define SHARE_MEMORY_0_SIZE             0x1000
#define SHARE_MEMORY_1_SIZE             0x1000
#define SHARE_MEMORY_2_SIZE             0x1000
#define SHARE_MEMORY_3_SIZE             0x1000
#define SHARE_MEMORY_4_SIZE             0x1000
#define SHARE_MEMORY_5_SIZE             0x1000
#define SHARE_MEMORY_6_SIZE             0x1000
#define SHARE_MEMORY_7_SIZE             0x1000

/* CIP SPU File */
#define CIP_F_ID                      0x1540
#define CIP_F_MODE                    0x1544
#define CIP_F_CTRL                    0x1548
#define CIP_F_SKIP                    0x154c
#define CIP_F_PAYLOAD                 0x1560

/* SBC1, 2 */
#define SBC_ACE_DMA_EN                0x6000
#define SBC_DMAPARAM21                0x6004
#define SBC_ACE_INTREN                0x6008
#define SBC_ACE_INTRST                0x600c
#define SBC_DMA_STATUS0               0x6010
#define SBC_DMA_STATUS1               0x6014
#define SBC_DMAPARAMA(i)              (0x6018 + (i) * 0x04)
#define   SBC_DMAPARAMA_OFFSET_MASK     GENMASK(31, 29)
#define   SBC_DMAPARAMA_LOOPADDR_MASK   GENMASK(28, 23)
#define   SBC_DMAPARAMA_COUNT_MASK      GENMASK(7, 0)
#define SBC_DMAPARAMB(i)              (0x6038 + (i) * 0x04)

/* IOB1, 2, 3 */
#define IOB_PKTCNT                    0x1740
#define IOB_PKTCNTRST                 0x1744
#define IOB_PKTCNTST                  0x1744
#define IOB_DUMMY_ENABLE              0x1748
#define IOB_FORMATCHANGE_EN           0x174c
#define IOB_UASSIST0                  0x1750
#define IOB_UASSIST1                  0x1754
#define IOB_URESERVE(i)               (0x1758 + (i) * 0x4)
#define IOB_PCRRECEN                  IOB_URESERVE(2)
#define IOB_UPARTIAL(i)               (0x1768 + (i) * 0x4)
#define IOB_SPUINTREN                 0x1778

#define IOB_HSCREV                    0x1a00
#define IOB_SECCLK(i)                 (0x1a08 + (i) * 0x6c)
#define IOB_SECTIMEH(i)               (0x1a0c + (i) * 0x6c)
#define IOB_SECTIMEL(i)               (0x1a10 + (i) * 0x6c)
#define IOB_RESET0                    0x1a14
#define   IOB_RESET0_APCORE             BIT(20)
#define IOB_RESET1                    0x1a18
#define IOB_CLKSTOP                   0x1a1c
#define IOB_DEBUG                     0x1a20
#define   IOB_DEBUG_SPUHALT             BIT(0)
#define IOB_INTREN(i)                 (0x1a24 + (i) * 0x8)
#define IOB_INTRST(i)                 (0x1a28 + (i) * 0x8)
#define IOB_INTREN0                   0x1a24
#define IOB_INTRST0                   0x1a28
#define IOB_INTREN0_1                 0x1a2c
#define IOB_INTRST0_1                 0x1a30
#define IOB_INTREN0_2                 0x1a34
#define IOB_INTRST0_2                 0x1a38
#define IOB_INTREN1                   0x1a3c
#define IOB_INTRST1                   0x1a40
#define IOB_INTREN1_1                 0x1a44
#define IOB_INTRST1_1                 0x1a48
#define IOB_INTREN2                   0x1a4c
#define IOB_INTRST2                   0x1a50
#define   INTR2_DRV                     BIT(31)
#define   INTR2_CIP_FRMT(i)             BIT((i) + 16)
#define   INTR2_CIP_NORMAL              BIT(16)
#define   INTR2_SEC_CLK_A               BIT(15)
#define   INTR2_SEC_CLK_S               BIT(14)
#define   INTR2_MBC_CIP_W(i)            BIT((i) + 9)
#define   INTR2_MBC_CIP_R(i)            BIT((i) + 4)
#define   INTR2_CIP_AUTH_A              BIT(1)
#define   INTR2_CIP_AUTH_S              BIT(0)
#define IOB_INTREN3                   0x1a54
#define IOB_INTRST3                   0x1a58
#define   INTR3_DRV                     BIT(31)
#define   INTR3_CIP_FRMT(i)             BIT((i) + 16)
#define   INTR3_SEC_CLK_A               BIT(15)
#define   INTR3_SEC_CLK_S               BIT(14)
#define   INTR3_MBC_CIP_W(i)            BIT((i) + 9)
#define   INTR3_MBC_CIP_R(i)            BIT((i) + 4)
#define   INTR3_CIP_AUTH_A              BIT(1)
#define   INTR3_CIP_AUTH_S              BIT(0)
#define IOB_INTREN4                   0x1a5c
#define IOB_INTRST4                   0x1a60

/* MBC1-7 Common */
#define CDMBC_STRT(i)                (0x2300 + ((i) - 1) * 0x4)
#define CDMBC_PERFCNFG               0x230c
#define CDMBC_STAT(i)                (0x2320 + (i) * 0x4)
#define CDMBC_PARTRESET(i)           (0x234c + (i) * 0x4)
#define CDMBC_MONNUM                 0x2358
#define CDMBC_MONDAT                 0x235c
#define CDMBC_PRC0CHIE0              0x2380
#define CDMBC_PRC0RBIE0              0x2384
#define CDMBC_PRC1CHIE0              0x2388
#define CDMBC_PRC2CHIE0              0x2390
#define CDMBC_PRC2RBIE0              0x2394
#define CDMBC_SOFTFLRQ               0x239c
#define CDMBC_TDSTRT                 0x23a0

#define INTR_MBC_CH_END              BIT(15)
#define INTR_MBC_CH_STOP             BIT(13)
#define INTR_MBC_CH_ADDR             BIT(6)
#define INTR_MBC_CH_IWDONE           BIT(3)
#define INTR_MBC_CH_WDONE            BIT(1)

/* MBC DMA channel, only for output DMA */
#define CDMBC_CHTDCTRLH(i)            (0x23a4 + (i) * 0x10)
#define   CDMBC_CHTDCTRLH_STREM_MASK    GENMASK(20, 16)
#define   CDMBC_CHTDCTRLH_NOT_FLT       BIT(7)
#define   CDMBC_CHTDCTRLH_ALL_EN        BIT(6)
#define CDMBC_CHTDCTRLU(i)            (0x23a8 + (i) * 0x10)

/* CIP file channel */
#define CDMBC_CIPMODE(i)              (0x24fc + (i) * 0x4)
#define   CDMBC_CIPMODE_PUSH            BIT(0)
#define CDMBC_CIPPRIORITY(i)          (0x2510 + (i) * 0x4)
#define   CDMBC_CIPPRIORITY_PRIOR_MASK  GENMASK(1, 0)
#define CDMBC_CH18ATTRIBUTE           (0x2524)

/* MBC DMA channel */
#define CDMBC_CHCTRL1(i)                  (0x2540 + (i) * 0x50)
#define   CDMBC_CHCTRL1_LINKCH1_MASK        GENMASK(12, 10)
#define   CDMBC_CHCTRL1_STATSEL_MASK        GENMASK(9, 7)
#define   CDMBC_CHCTRL1_TYPE_INTERMIT       BIT(1)
#define   CDMBC_CHCTRL1_IND_SIZE_UND        BIT(0)
#define CDMBC_CHCTRL2(i)                  (0x2544 + (i) * 0x50)
#define CDMBC_CHDDR(i)                    (0x2548 + (i) * 0x50)
#define   CDMBC_CHDDR_REG_LOAD_ON           BIT(4)
#define   CDMBC_CHDDR_AT_CHEN_ON            BIT(3)
#define   CDMBC_CHDDR_SET_MCB_MASK          GENMASK(2, 1)
#define   CDMBC_CHDDR_SET_MCB_WR            (0x0 << 1)
#define   CDMBC_CHDDR_SET_MCB_RD            (0x3 << 1)
#define   CDMBC_CHDDR_SET_DDR_1             BIT(0)
#define CDMBC_CHCAUSECTRL(i)              (0x254c + (i) * 0x50)
#define   CDMBC_CHCAUSECTRL_MODE_MASK       BIT(31)
#define   CDMBC_CHCAUSECTRL_CSEL2_MASK      GENMASK(20, 12)
#define   CDMBC_CHCAUSECTRL_CSEL1_MASK      GENMASK(8, 0)
#define CDMBC_CHSTAT(i)                   (0x2550 + (i) * 0x50)
#define CDMBC_CHIR(i)                     (0x2554 + (i) * 0x50)
#define CDMBC_CHIE(i)                     (0x2558 + (i) * 0x50)
#define CDMBC_CHID(i)                     (0x255c + (i) * 0x50)
#define   CDMBC_CHI_STOPPED                 BIT(13)
#define   CDMBC_CHI_TRANSIT                 BIT(6)
#define   CDMBC_CHI_STARTING                BIT(1)
#define CDMBC_CHSRCAMODE(i)               (0x2560 + (i) * 0x50)
#define CDMBC_CHDSTAMODE(i)               (0x2564 + (i) * 0x50)
#define   CDMBC_CHAMODE_TUNIT_MASK          GENMASK(29, 28)
#define   CDMBC_CHAMODE_ENDIAN_MASK         GENMASK(17, 16)
#define   CDMBC_CHAMODE_AUPDT_MASK          GENMASK(5, 4)
#define   CDMBC_CHAMODE_TYPE_RB             BIT(2)
#define CDMBC_CHSRCSTRTADRSD(i)           (0x2568 + (i) * 0x50)
#define CDMBC_CHSRCSTRTADRSU(i)           (0x256c + (i) * 0x50)
#define CDMBC_CHDSTSTRTADRSD(i)           (0x2570 + (i) * 0x50)
#define CDMBC_CHDSTSTRTADRSU(i)           (0x2574 + (i) * 0x50)
#define   CDMBC_CHDSTSTRTADRS_TID_MASK      GENMASK(31, 28)
#define   CDMBC_CHDSTSTRTADRS_ID1_EN_MASK   BIT(15)
#define   CDMBC_CHDSTSTRTADRS_KEY_ID1_MASK  GENMASK(12, 8)
#define   CDMBC_CHDSTSTRTADRS_KEY_ID0_MASK  GENMASK(4, 0)
#define CDMBC_CHSIZE(i)                   (0x2578 + (i) * 0x50)
#define CDMBC_CHIRADRSD(i)                (0x2580 + (i) * 0x50)
#define CDMBC_CHIRADRSU(i)                (0x2584 + (i) * 0x50)
#define CDMBC_CHDST1STUSIZE(i)            (0x258C + (i) * 0x50)

/* MBC DMA intermit transfer, only for input DMA */
#define CDMBC_ITCTRL(i)              (0x3000 + (i) * 0x20)
#define CDMBC_ITSTEPS(i)             (0x3018 + (i) * 0x20)

/* MBC ring buffer */
#define CDMBC_RBBGNADRS(i)           (0x3200 + (i) * 0x40)
#define CDMBC_RBBGNADRSD(i)          (0x3200 + (i) * 0x40)
#define CDMBC_RBBGNADRSU(i)          (0x3204 + (i) * 0x40)
#define CDMBC_RBENDADRS(i)           (0x3208 + (i) * 0x40)
#define CDMBC_RBENDADRSD(i)          (0x3208 + (i) * 0x40)
#define CDMBC_RBENDADRSU(i)          (0x320C + (i) * 0x40)
#define CDMBC_RBIR(i)                (0x3214 + (i) * 0x40)
#define CDMBC_RBIE(i)                (0x3218 + (i) * 0x40)
#define CDMBC_RBID(i)                (0x321c + (i) * 0x40)
#define CDMBC_RBRDPTR(i)             (0x3220 + (i) * 0x40)
#define CDMBC_RBRDPTRD(i)            (0x3220 + (i) * 0x40)
#define CDMBC_RBRDPTRU(i)            (0x3224 + (i) * 0x40)
#define CDMBC_RBWRPTR(i)             (0x3228 + (i) * 0x40)
#define CDMBC_RBWRPTRD(i)            (0x3228 + (i) * 0x40)
#define CDMBC_RBWRPTRU(i)            (0x322C + (i) * 0x40)
#define CDMBC_RBERRCNFG(i)           (0x3238 + (i) * 0x40)

/* MBC Rate */
#define CDMBC_RCNMSKCYC(i)           (MBC6_TOP_ADDR + 0x000 + (i) * 0x04)

/* MBC Address Transfer */
#define CDMBC_CHPSIZE(i)             (0x3c00 + ((i) - 1) * 0x48)
#define CDMBC_CHATCTRL(i)            (0x3c04 + ((i) - 1) * 0x48)
#define CDMBC_CHBTPAGE(i, j)         (0x3c08 + ((i) - 1) * 0x48 + (j) * 0x10)
#define CDMBC_CHBTPAGED(i, j)        (0x3c08 + ((i) - 1) * 0x48 + (j) * 0x10)
#define CDMBC_CHBTPAGEU(i, j)        (0x3c0C + ((i) - 1) * 0x48 + (j) * 0x10)
#define CDMBC_CHATPAGE(i, j)         (0x3c10 + ((i) - 1) * 0x48 + (j) * 0x10)
#define CDMBC_CHATPAGED(i, j)        (0x3c10 + ((i) - 1) * 0x48 + (j) * 0x10)
#define CDMBC_CHATPAGEU(i, j)        (0x3c14 + ((i) - 1) * 0x48 + (j) * 0x10)

/* CSS */
#define CSS_PTSOCONFIG                   0x1c00
#define CSS_PTSISIGNALPOL                0x1c04
#define CSS_SIGNALPOLCH(i)               (0x1c08 + (i) * 0x4)
#define CSS_OUTPUTENABLE                 0x1c10
#define CSS_OUTPUTCTRL(i)                (0x1c14 + (i) * 0x4)
#define CSS_STSOCONFIG                   0x1c2c
#define CSS_STSOSIGNALPOL                0x1c30
#define CSS_DMDSIGNALPOL                 0x1c34
#define CSS_PTSOSIGNALPOL                0x1c38
#define CSS_PF0CONFIG                    0x1c3c
#define CSS_PF1CONFIG                    0x1c40
#define CSS_PFINTENABLE                  0x1c44
#define CSS_PFINTSTATUS                  0x1c48
#define CSS_AVOUTPUTCTRL(i)              (0x1c4c + (i) * 0x4)
#define CSS_DPCTRL(i)                    (0x1c54 + (i) * 0x4)
#define   CSS_DPCTRL_DPSEL_MASK            GENMASK(22, 0)
#define   CSS_DPCTRL_DPSEL_PLAY5           BIT(15)
#define   CSS_DPCTRL_DPSEL_PLAY4           BIT(14)
#define   CSS_DPCTRL_DPSEL_PLAY3           BIT(13)
#define   CSS_DPCTRL_DPSEL_PLAY2           BIT(12)
#define   CSS_DPCTRL_DPSEL_PLAY1           BIT(11)
#define   CSS_DPCTRL_DPSEL_PLAY0           BIT(10)
#define   CSS_DPCTRL_DPSEL_TSI4            BIT(4)
#define   CSS_DPCTRL_DPSEL_TSI3            BIT(3)
#define   CSS_DPCTRL_DPSEL_TSI2            BIT(2)
#define   CSS_DPCTRL_DPSEL_TSI1            BIT(1)
#define   CSS_DPCTRL_DPSEL_TSI0            BIT(0)

/* TSI */
#define TSI_SYNCCNTROL(i)                (0x7100 + (i) * 0x70)
#define   TSI_SYNCCNTROL_FRAME_MASK        GENMASK(18, 16)
#define   TSI_SYNCCNTROL_FRAME_EXTSYNC1    (0x0 << 16)
#define   TSI_SYNCCNTROL_FRAME_EXTSYNC2    (0x1 << 16)
#define TSI_CONFIG(i)                    (0x7104 + (i) * 0x70)
#define   TSI_CONFIG_ATSMD_MASK            GENMASK(22, 21)
#define   TSI_CONFIG_ATSMD_PCRPLL0         (0x0 << 21)
#define   TSI_CONFIG_ATSMD_PCRPLL1         (0x1 << 21)
#define   TSI_CONFIG_ATSMD_DPLL            (0x3 << 21)
#define   TSI_CONFIG_ATSADD_ON             BIT(20)
#define   TSI_CONFIG_STCMD_MASK            GENMASK(7, 6)
#define   TSI_CONFIG_STCMD_PCRPLL0         (0x0 << 6)
#define   TSI_CONFIG_STCMD_PCRPLL1         (0x1 << 6)
#define   TSI_CONFIG_STCMD_DPLL            (0x3 << 6)
#define   TSI_CONFIG_CHEN_START            BIT(0)
#define TSI_RATEUPLMT(i)                 (0x7108 + (i) * 0x70)
#define TSI_RATELOWLMT(i)                (0x710c + (i) * 0x70)
#define TSI_CNTINTR(i)                   (0x7110 + (i) * 0x70)
#define TSI_INTREN(i)                    (0x7114 + (i) * 0x70)
#define   TSI_INTR_NTP                     BIT(13)
#define   TSI_INTR_NTPCNT                  BIT(12)
#define   TSI_INTR_PKTEND                  BIT(11)
#define   TSI_INTR_PCR                     BIT(9)
#define   TSI_INTR_LOAD                    BIT(8)
#define   TSI_INTR_SERR                    BIT(7)
#define   TSI_INTR_SOF                     BIT(6)
#define   TSI_INTR_TOF                     BIT(5)
#define   TSI_INTR_UL                      BIT(4)
#define   TSI_INTR_LL                      BIT(3)
#define   TSI_INTR_CNT                     BIT(2)
#define   TSI_INTR_LOST                    BIT(1)
#define   TSI_INTR_LOCK                    BIT(0)
#define TSI_SYNCSTATUS(i)                (0x7118 + (i) * 0x70)
#define   TSI_STAT_PKTST_ERR               BIT(21)
#define   TSI_STAT_LARGE_ERR               BIT(20)
#define   TSI_STAT_SMALL_ERR               BIT(19)
#define   TSI_STAT_LOCK                    BIT(18)
#define   TSI_STAT_SYNC                    BIT(17)
#define   TSI_STAT_SEARCH                  BIT(16)

/* UCODE DL */
#define UCODE_REVISION_AM                0x10fd0
#define CIP_UCODEADDR_AM1                0x10fd4
#define CIP_UCODEADDR_AM0                0x10fd8
#define CORRECTATS_CTRL                  0x10fdc
#define UCODE_REVISION                   0x10fe0
#define AM_UCODE_IGPGCTRL                0x10fe4
#define REPDPLLCTRLEN                    0x10fe8
#define UCODE_DLADDR1                    0x10fec
#define UCODE_DLADDR0                    0x10ff0
#define UCODE_ERRLOGCTRL                 0x10ff4

#endif /* DVB_UNIPHIER_HSC_REG_H__ */
