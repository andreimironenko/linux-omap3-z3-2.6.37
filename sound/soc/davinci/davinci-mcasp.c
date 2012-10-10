/*
 * ALSA SoC McASP Audio Layer for TI DAVINCI processor
 *
 * Multi-channel Audio Serial Port Driver
 *
 * Author: Nirmal Pandey <n-pandey@ti.com>,
 *         Suresh Rajashekara <suresh.r@ti.com>
 *         Steve Chen <schen@.mvista.com>
 *
 * Copyright:   (C) 2009 MontaVista Software, Inc., <source@mvista.com>
 * Copyright:   (C) 2009  Texas Instruments, India
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "davinci-pcm.h"
#include "davinci-mcasp.h"

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#include <mach/z3_fpga.h>
#include <mach/z3_app.h>
#endif

/*
 * McASP register definitions
 */
#define DAVINCI_MCASP_PID_REG		0x00
#define DAVINCI_MCASP_PWREMUMGT_REG	0x04

#define DAVINCI_MCASP_PFUNC_REG		0x10
#define DAVINCI_MCASP_PDIR_REG		0x14
#define DAVINCI_MCASP_PDOUT_REG		0x18
#define DAVINCI_MCASP_PDSET_REG		0x1c

#define DAVINCI_MCASP_PDCLR_REG		0x20

#define DAVINCI_MCASP_TLGC_REG		0x30
#define DAVINCI_MCASP_TLMR_REG		0x34

#define DAVINCI_MCASP_GBLCTL_REG	0x44
#define DAVINCI_MCASP_AMUTE_REG		0x48
#define DAVINCI_MCASP_LBCTL_REG		0x4c

#define DAVINCI_MCASP_TXDITCTL_REG	0x50

#define DAVINCI_MCASP_GBLCTLR_REG	0x60
#define DAVINCI_MCASP_RXMASK_REG	0x64
#define DAVINCI_MCASP_RXFMT_REG		0x68
#define DAVINCI_MCASP_RXFMCTL_REG	0x6c

#define DAVINCI_MCASP_ACLKRCTL_REG	0x70
#define DAVINCI_MCASP_AHCLKRCTL_REG	0x74
#define DAVINCI_MCASP_RXTDM_REG		0x78
#define DAVINCI_MCASP_EVTCTLR_REG	0x7c

#define DAVINCI_MCASP_RXSTAT_REG	0x80
#define DAVINCI_MCASP_RXTDMSLOT_REG	0x84
#define DAVINCI_MCASP_RXCLKCHK_REG	0x88
#define DAVINCI_MCASP_REVTCTL_REG	0x8c

#define DAVINCI_MCASP_GBLCTLX_REG	0xa0
#define DAVINCI_MCASP_TXMASK_REG	0xa4
#define DAVINCI_MCASP_TXFMT_REG		0xa8
#define DAVINCI_MCASP_TXFMCTL_REG	0xac

#define DAVINCI_MCASP_ACLKXCTL_REG	0xb0
#define DAVINCI_MCASP_AHCLKXCTL_REG	0xb4
#define DAVINCI_MCASP_TXTDM_REG		0xb8
#define DAVINCI_MCASP_EVTCTLX_REG	0xbc

#define DAVINCI_MCASP_TXSTAT_REG	0xc0
#define DAVINCI_MCASP_TXTDMSLOT_REG	0xc4
#define DAVINCI_MCASP_TXCLKCHK_REG	0xc8
#define DAVINCI_MCASP_XEVTCTL_REG	0xcc

/* Left(even TDM Slot) Channel Status Register File */
#define DAVINCI_MCASP_DITCSRA_REG	0x100
/* Right(odd TDM slot) Channel Status Register File */
#define DAVINCI_MCASP_DITCSRB_REG	0x118
/* Left(even TDM slot) User Data Register File */
#define DAVINCI_MCASP_DITUDRA_REG	0x130
/* Right(odd TDM Slot) User Data Register File */
#define DAVINCI_MCASP_DITUDRB_REG	0x148

/* Serializer n Control Register */
#define DAVINCI_MCASP_XRSRCTL_BASE_REG	0x180
#define DAVINCI_MCASP_XRSRCTL_REG(n)	(DAVINCI_MCASP_XRSRCTL_BASE_REG + \
						(n << 2))

/* Transmit Buffer for Serializer n */
#define DAVINCI_MCASP_TXBUF_REG		0x200
/* Receive Buffer for Serializer n */
#define DAVINCI_MCASP_RXBUF_REG		0x280

/* McASP FIFO Registers */
#ifndef CONFIG_ARCH_TI81XX
#define DAVINCI_MCASP_WFIFOCTL		(0x1010)
#define DAVINCI_MCASP_WFIFOSTS		(0x1014)
#define DAVINCI_MCASP_RFIFOCTL		(0x1018)
#define DAVINCI_MCASP_RFIFOSTS		(0x101C)
#else
#define DAVINCI_MCASP_WFIFOCTL		(0x1000)
#define DAVINCI_MCASP_WFIFOSTS		(0x1004)
#define DAVINCI_MCASP_RFIFOCTL		(0x1008)
#define DAVINCI_MCASP_RFIFOSTS		(0x100C)
#endif

/*
 * DAVINCI_MCASP_PWREMUMGT_REG - Power Down and Emulation Management
 *     Register Bits
 */
#define MCASP_FREE	BIT(0)
#define MCASP_SOFT	BIT(1)

/*
 * DAVINCI_MCASP_PFUNC_REG - Pin Function / GPIO Enable Register Bits
 */
#define AXR(n)		(1<<n)
#define PFUNC_AMUTE	BIT(25)
#define ACLKX		BIT(26)
#define AHCLKX		BIT(27)
#define AFSX		BIT(28)
#define ACLKR		BIT(29)
#define AHCLKR		BIT(30)
#define AFSR		BIT(31)

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define MCASP_BITCLK_FS_PINS_MASK (ACLKX|ACLKR|AFSX|AFSR)
#endif

/*
 * DAVINCI_MCASP_PDIR_REG - Pin Direction Register Bits
 */
#define AXR(n)		(1<<n)
#define PDIR_AMUTE	BIT(25)
#define ACLKX		BIT(26)
#define AHCLKX		BIT(27)
#define AFSX		BIT(28)
#define ACLKR		BIT(29)
#define AHCLKR		BIT(30)
#define AFSR		BIT(31)

/*
 * DAVINCI_MCASP_TXDITCTL_REG - Transmit DIT Control Register Bits
 */
#define DITEN	BIT(0)	/* Transmit DIT mode enable/disable */
#define VA	BIT(2)
#define VB	BIT(3)

/*
 * DAVINCI_MCASP_TXFMT_REG - Transmit Bitstream Format Register Bits
 */
#define TXROT(val)	(val)
#define TXSEL		BIT(3)
#define TXSSZ(val)	(val<<4)
#define TXPBIT(val)	(val<<8)
#define TXPAD(val)	(val<<13)
#define TXORD		BIT(15)
#define FSXDLY(val)	(val<<16)

/*
 * DAVINCI_MCASP_RXFMT_REG - Receive Bitstream Format Register Bits
 */
#define RXROT(val)	(val)
#define RXSEL		BIT(3)
#define RXSSZ(val)	(val<<4)
#define RXPBIT(val)	(val<<8)
#define RXPAD(val)	(val<<13)
#define RXORD		BIT(15)
#define FSRDLY(val)	(val<<16)

/*
 * DAVINCI_MCASP_TXFMCTL_REG -  Transmit Frame Control Register Bits
 */
#define FSXPOL		BIT(0)
#define AFSXE		BIT(1)
#define FSXDUR		BIT(4)
#define FSXMOD(val)	(val<<7)

/*
 * DAVINCI_MCASP_RXFMCTL_REG - Receive Frame Control Register Bits
 */
#define FSRPOL		BIT(0)
#define AFSRE		BIT(1)
#define FSRDUR		BIT(4)
#define FSRMOD(val)	(val<<7)

/*
 * DAVINCI_MCASP_ACLKXCTL_REG - Transmit Clock Control Register Bits
 */
#define ACLKXDIV(val)	(val)
#define ACLKXE		BIT(5)
#define TX_ASYNC	BIT(6)
#define ACLKXPOL	BIT(7)

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define ACLKXDIVMASK    0x1f
#endif
/*
 * DAVINCI_MCASP_ACLKRCTL_REG Receive Clock Control Register Bits
 */
#define ACLKRDIV(val)	(val)
#define ACLKRE		BIT(5)
#define RX_ASYNC	BIT(6)
#define ACLKRPOL	BIT(7)

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define ACLKRDIVMASK    0x1f
#endif
/*
 * DAVINCI_MCASP_AHCLKXCTL_REG - High Frequency Transmit Clock Control
 *     Register Bits
 */
#define AHCLKXDIV(val)	(val)
#define AHCLKXPOL	BIT(14)
#define AHCLKXE		BIT(15)

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define AHCLKXDIVMASK   0xfff
#endif

/*
 * DAVINCI_MCASP_AHCLKRCTL_REG - High Frequency Receive Clock Control
 *     Register Bits
 */
#define AHCLKRDIV(val)	(val)
#define AHCLKRPOL	BIT(14)
#define AHCLKRE		BIT(15)
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define AHCLKRDIVMASK   0xfff
#endif

/*
 * DAVINCI_MCASP_XRSRCTL_BASE_REG -  Serializer Control Register Bits
 */
#define MODE(val)	(val)
#define DISMOD		(val)(val<<2)
#define TXSTATE		BIT(4)
#define RXSTATE		BIT(5)

/*
 * DAVINCI_MCASP_LBCTL_REG - Loop Back Control Register Bits
 */
#define LBEN		BIT(0)
#define LBORD		BIT(1)
#define LBGENMODE(val)	(val<<2)

/*
 * DAVINCI_MCASP_TXTDMSLOT_REG - Transmit TDM Slot Register configuration
 */
#define TXTDMS(n)	(1<<n)

/*
 * DAVINCI_MCASP_RXTDMSLOT_REG - Receive TDM Slot Register configuration
 */
#define RXTDMS(n)	(1<<n)

/*
 * DAVINCI_MCASP_GBLCTL_REG -  Global Control Register Bits
 */
#define RXCLKRST	BIT(0)	/* Receiver Clock Divider Reset */
#define RXHCLKRST	BIT(1)	/* Receiver High Frequency Clock Divider */
#define RXSERCLR	BIT(2)	/* Receiver Serializer Clear */
#define RXSMRST		BIT(3)	/* Receiver State Machine Reset */
#define RXFSRST		BIT(4)	/* Frame Sync Generator Reset */
#define TXCLKRST	BIT(8)	/* Transmitter Clock Divider Reset */
#define TXHCLKRST	BIT(9)	/* Transmitter High Frequency Clock Divider*/
#define TXSERCLR	BIT(10)	/* Transmit Serializer Clear */
#define TXSMRST		BIT(11)	/* Transmitter State Machine Reset */
#define TXFSRST		BIT(12)	/* Frame Sync Generator Reset */

/*
 * DAVINCI_MCASP_AMUTE_REG -  Mute Control Register Bits
 */
#define MUTENA(val)	(val)
#define MUTEINPOL	BIT(2)
#define MUTEINENA	BIT(3)
#define MUTEIN		BIT(4)
#define MUTER		BIT(5)
#define MUTEX		BIT(6)
#define MUTEFSR		BIT(7)
#define MUTEFSX		BIT(8)
#define MUTEBADCLKR	BIT(9)
#define MUTEBADCLKX	BIT(10)
#define MUTERXDMAERR	BIT(11)
#define MUTETXDMAERR	BIT(12)

/*
 * DAVINCI_MCASP_REVTCTL_REG - Receiver DMA Event Control Register bits
 */
#define RXDATADMADIS	BIT(0)

/*
 * DAVINCI_MCASP_XEVTCTL_REG - Transmitter DMA Event Control Register bits
 */
#define TXDATADMADIS	BIT(0)

/*
 * DAVINCI_MCASP_W[R]FIFOCTL - Write/Read FIFO Control Register bits
 */
#define FIFO_ENABLE	BIT(16)
#define NUMEVT_MASK	(0xFF << 8)
#define NUMDMA_MASK	(0xFF)

#define DAVINCI_MCASP_NUM_SERIALIZER	16

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#define DAVINCI_CPU_MAX_MCASP           3
#endif

static inline void mcasp_set_bits(void __iomem *reg, u32 val)
{
	__raw_writel(__raw_readl(reg) | val, reg);
}

static inline void mcasp_clr_bits(void __iomem *reg, u32 val)
{
	__raw_writel((__raw_readl(reg) & ~(val)), reg);
}

static inline void mcasp_mod_bits(void __iomem *reg, u32 val, u32 mask)
{
	__raw_writel((__raw_readl(reg) & ~mask) | val, reg);
}

static inline void mcasp_set_reg(void __iomem *reg, u32 val)
{
	__raw_writel(val, reg);
}

static inline u32 mcasp_get_reg(void __iomem *reg)
{
	return (unsigned int)__raw_readl(reg);
}

static inline void mcasp_set_ctl_reg(void __iomem *regs, u32 val)
{
	int i = 0;

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
/* Look in real GBLCTL_REG for update - shadow reg is not accurate */
        void __iomem *gblctl_reg = (void __iomem *) ((((unsigned long)regs) & 0xffffff00) + DAVINCI_MCASP_GBLCTL_REG);
#endif
	mcasp_set_bits(regs, val);

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()

	/* programming GBLCTL needs to read back from GBLCTL and verfiy */
	/* loop count is to avoid the lock-up */
	for (i = 0; i < 1000; i++) {
		if ((mcasp_get_reg(regs) & val) == val)
			break;
	}

	if (i == 1000 && ((mcasp_get_reg(regs) & val) != val))
		printk(KERN_ERR "GBLCTL write error\n");
#else

	/* programming GBLCTL needs to read back from GBLCTL and verfiy */
	/* loop count is to avoid the lock-up */
	for (i = 0; i < 100; i++) {
		if ((mcasp_get_reg(gblctl_reg) & val) == val)
			break;
                udelay(1);
	}

	if (i == 100 && ((mcasp_get_reg(gblctl_reg) & val) != val))
		printk(KERN_ERR "GBLCTL write error\n");

#endif //!defined(CONFIG_MACH_Z3_DM816X_MOD) || !defined(CONFIG_MACH_Z3_DM814X_MOD)
}

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
static void mcasp_set_board_clks(struct davinci_audio_dev *dev)
{
        int board_id = z3_fpga_board_id();

        switch ( dev->id )
        {
        case 0:
        default:
                dev->mclk_out = 0;
                dev->use_tx_clk_for_rx = 0;
                break;
        case 1:
                dev->mclk_out = AHCLKR|AHCLKX;
                dev->use_tx_clk_for_rx = 0;
                break;
        case 2:
                switch ( board_id ) 
                {
                case Z3_BOARD_ID_APP_31:
                        if (z3_fpga_get_aic_disable() == 0 ) {
                                if (z3_fpga_get_aic_ext_master() == 0 ) {
                                        dev->mclk_out = AHCLKX;
                                } else {
                                        dev->mclk_out = 0;
                                }
                                dev->use_tx_clk_for_rx = 1;
                        } else {
                                dev->mclk_out = 0;
                                dev->use_tx_clk_for_rx = 1;
                        }
                        break;

                default:
                        dev->mclk_out = AHCLKX;
                        dev->use_tx_clk_for_rx = 1;
                        break;
                }
                break;
        }
}

static void mcasp_enable_mclk(struct davinci_audio_dev *dev)
{
        mcasp_set_board_clks(dev);

        if ( AHCLKX == (AHCLKX & dev->mclk_out ) ) {
                // on the Z3 board, the AHCLKX is used as the MCLK to the codec
                // so we have to turn it on even for rx
                mcasp_mod_bits( dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXDIV(7), AHCLKXDIVMASK);
//                mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXDIV(0), ACLKXDIVMASK);
                //printk("turn on txhclk with glbctlx reg\n");
                mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKX);

                mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXHCLKRST);
                //printk("turn on txhclk - done\n");
        } else {
                mcasp_mod_bits( dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXDIV(0), AHCLKXDIVMASK);
                mcasp_clr_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKX);
                mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXHCLKRST);
        }

        if ( AHCLKR == (AHCLKR & dev->mclk_out ) ) {
                mcasp_mod_bits( dev->base + DAVINCI_MCASP_AHCLKRCTL_REG, AHCLKRDIV(7), AHCLKRDIVMASK);
                mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKR);
                mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXHCLKRST);
        } else {
                mcasp_mod_bits( dev->base + DAVINCI_MCASP_AHCLKRCTL_REG, AHCLKRDIV(0), AHCLKRDIVMASK);
                mcasp_clr_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AHCLKR);
                mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXHCLKRST);
        }
}

#endif //machine_is_z3_816x_mod() || machine_is_z3_814x_mod()

static void mcasp_start_rx(struct davinci_audio_dev *dev)
{
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXHCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSERCLR);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXFSRST);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXFSRST);
}

static void mcasp_start_tx(struct davinci_audio_dev *dev)
{
	u8 offset = 0, i;
	u32 cnt;

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    mcasp_enable_mclk( dev );

	mcasp_mod_bits( dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXDIV(7), AHCLKXDIVMASK);
	mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXDIV(dev->aclkdiv), ACLKXDIVMASK);

    mcasp_mod_bits(dev->base + DAVINCI_MCASP_PDIR_REG, dev->pdir_tx_mask, MCASP_BITCLK_FS_PINS_MASK);
#endif //defined(CONFIG_MACH_Z3_DM816X_MOD) || defined(CONFIG_MACH_Z3_DM814X_MOD)

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXHCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXCLKRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXSERCLR);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);

	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXSMRST);
	mcasp_set_ctl_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXFSRST);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);
	for (i = 0; i < dev->num_serializer; i++) {
		if (dev->serial_dir[i] == TX_MODE) {
			offset = i;
			break;
		}
	}

	/* wait for TX ready */
	cnt = 0;
	while (!(mcasp_get_reg(dev->base + DAVINCI_MCASP_XRSRCTL_REG(offset)) &
		 TXSTATE) && (cnt < 100000))
		cnt++;

	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXBUF_REG, 0);
}

static void davinci_mcasp_start(struct davinci_audio_dev *dev, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt)	/* enable FIFO */
			mcasp_set_bits(dev->base + DAVINCI_MCASP_WFIFOCTL,
								FIFO_ENABLE);
		mcasp_start_tx(dev);
	} else {
		if (dev->rxnumevt)	/* enable FIFO */
			mcasp_set_bits(dev->base + DAVINCI_MCASP_RFIFOCTL,
								FIFO_ENABLE);
		mcasp_start_rx(dev);
	}
}

static void mcasp_stop_rx(struct davinci_audio_dev *dev)
{
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
  if ( AHCLKR == (AHCLKR & dev->mclk_out ) ) {
          mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, RXHCLKRST);
        } else {
                mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, 0);
        }
#else
	mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLR_REG, 0);
#endif

	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG, 0xFFFFFFFF);
}

static void mcasp_stop_tx(struct davinci_audio_dev *dev)
{
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    mcasp_mod_bits(dev->base + DAVINCI_MCASP_PDIR_REG, 0, MCASP_BITCLK_FS_PINS_MASK);
        if ( AHCLKX == (AHCLKX & dev->mclk_out ) ) {
                mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, TXHCLKRST);
        } else {
                mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, 0);
        }
#else
  mcasp_set_reg(dev->base + DAVINCI_MCASP_GBLCTLX_REG, 0);
#endif
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG, 0xFFFFFFFF);
}

static void davinci_mcasp_stop(struct davinci_audio_dev *dev, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt)	/* disable FIFO */
			mcasp_clr_bits(dev->base + DAVINCI_MCASP_WFIFOCTL,
								FIFO_ENABLE);
		mcasp_stop_tx(dev);
	} else {
		if (dev->rxnumevt)	/* disable FIFO */
			mcasp_clr_bits(dev->base + DAVINCI_MCASP_RFIFOCTL,
								FIFO_ENABLE);
		mcasp_stop_rx(dev);
	}
}

static int davinci_mcasp_set_dai_fmt(struct snd_soc_dai *cpu_dai,
					 unsigned int fmt)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	void __iomem *base = dev->base;
	
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
	u32 normal_fs_polarity;
    u32 inverted_fs_polarity;

        
    mcasp_enable_mclk( dev );

    dev->pdir_tx_mask = 0;
#endif

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()

case SND_SOC_DAIFMT_CBS_CFM:
		/* codec is clock and frame slave */
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);

//		mcasp_mod_bits(base + DAVINCI_MCASP_PDIR_REG, , MCASP_BITCLK_FS_PINS_MASK);
        dev->pdir_tx_mask = (ACLKX|ACLKR);
                
        mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRDIV(dev->aclkdiv), ACLKRDIVMASK);
        mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXDIV(dev->aclkdiv), ACLKXDIVMASK);

		break;
#endif
	case SND_SOC_DAIFMT_CBS_CFS:
		/* codec is clock and frame slave */
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);
		
#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
		mcasp_set_bits(base + DAVINCI_MCASP_PDIR_REG, (0x7 << 26));
#else
        dev->pdir_tx_mask = MCASP_BITCLK_FS_PINS_MASK;

        mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRDIV(dev->aclkdiv), ACLKRDIVMASK);
        mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXDIV(dev->aclkdiv), ACLKXDIVMASK);
#endif
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		/* codec is clock master and frame slave */
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
		mcasp_set_bits(base + DAVINCI_MCASP_PDIR_REG, (0x2d << 26));
#else
        mcasp_mod_bits(base + DAVINCI_MCASP_PDIR_REG, (AFSX|AFSR), MCASP_BITCLK_FS_PINS_MASK);
#endif
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		/* codec is clock and frame master */
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXE);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, AFSXE);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRE);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, AFSRE);
		
#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
		mcasp_clr_bits(base + DAVINCI_MCASP_PDIR_REG, (0x3f << 26));
#else
        mcasp_mod_bits(base + DAVINCI_MCASP_PDIR_REG, 0, MCASP_BITCLK_FS_PINS_MASK);
#endif
		break;

	default:
		return -EINVAL;
	}

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()

switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK ) ) {
	case SND_SOC_DAIFMT_I2S:
                normal_fs_polarity = FSXPOL;
                inverted_fs_polarity = 0;
                break;
        default:
                normal_fs_polarity = 0;
                inverted_fs_polarity = FSXPOL;
                break;
        }
#endif

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_IF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_IB_IF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_set_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_NF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_TXFMCTL_REG, FSXPOL);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_clr_bits(base + DAVINCI_MCASP_RXFMCTL_REG, FSRPOL);
		break;
#else
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_TXFMCTL_REG, normal_fs_polarity, FSXPOL);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_RXFMCTL_REG, normal_fs_polarity, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_IF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_TXFMCTL_REG, inverted_fs_polarity, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_RXFMCTL_REG, inverted_fs_polarity, FSRPOL);
		break;

	case SND_SOC_DAIFMT_IB_IF:
		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_TXFMCTL_REG, inverted_fs_polarity, FSXPOL);

		mcasp_clr_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_RXFMCTL_REG, inverted_fs_polarity, FSRPOL);
		break;

	case SND_SOC_DAIFMT_NB_NF:
		mcasp_set_bits(base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_TXFMCTL_REG, normal_fs_polarity, FSXPOL);

		mcasp_set_bits(base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRPOL);
		mcasp_mod_bits(base + DAVINCI_MCASP_RXFMCTL_REG, normal_fs_polarity, FSRPOL);
		break;
#endif  //!machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()


	default:
		return -EINVAL;
		}
		
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK ) ) {
	case SND_SOC_DAIFMT_I2S:
        default:
		mcasp_set_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG, FSRDUR);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG, FSXDUR);
                break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG, FSRDUR);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG, FSXDUR);
                break;
        }

	switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK ) ) {
	case SND_SOC_DAIFMT_I2S:
                mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, FSRDLY(1), FSRDLY(0x3));
                mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, FSXDLY(1), FSXDLY(0x3));
                break;

        default:
                mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, FSRDLY(0), FSRDLY(0x3));
                mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, FSXDLY(0), FSXDLY(0x3));
                break;
	}
#endif //machine_is_z3_816x_mod() || machine_is_z3_814x_mod()

	return 0;
}

static int davinci_config_channel_size(struct davinci_audio_dev *dev,
				       int channel_size)
{
	u32 fmt = 0;
	u32 mask, rotate;

	switch (channel_size) {
	case DAVINCI_AUDIO_WORD_8:
		fmt = 0x03;
		rotate = 6;
		mask = 0x000000ff;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0x1f;
#endif
		break;

	case DAVINCI_AUDIO_WORD_12:
		fmt = 0x05;
		rotate = 5;
		mask = 0x00000fff;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0xf;
#endif
		break;

	case DAVINCI_AUDIO_WORD_16:
		fmt = 0x07;
		rotate = 4;
		mask = 0x0000ffff;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0xf;
#endif
		break;

	case DAVINCI_AUDIO_WORD_20:
		fmt = 0x09;
		rotate = 3;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0x7;
#endif
		mask = 0x000fffff;
		break;

	case DAVINCI_AUDIO_WORD_24:
		fmt = 0x0B;
		rotate = 2;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0x7;
#endif
		mask = 0x00ffffff;
		break;

	case DAVINCI_AUDIO_WORD_28:
		fmt = 0x0D;
		rotate = 1;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0x7;
#endif
		mask = 0x0fffffff;
		break;

	case DAVINCI_AUDIO_WORD_32:
		fmt = 0x0F;
		rotate = 0;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        dev->aclkdiv = 0x7;
#endif
		mask = 0xffffffff;
		break;

	default:
		return -EINVAL;
}
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
  if ( dev->rrot_nibbles_additional ) {
                rotate += dev->rrot_nibbles_additional;
                rotate &= 7;
	}
#endif

	mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG,
					RXSSZ(fmt), RXSSZ(0x0F));
	mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG,
					TXSSZ(fmt), TXSSZ(0x0F));
	mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, TXROT(rotate),
							TXROT(7));
	mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, RXROT(rotate),
							RXROT(7));
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXMASK_REG, mask);
	mcasp_set_reg(dev->base + DAVINCI_MCASP_RXMASK_REG, mask);

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKRCTL_REG, ACLKRDIV(dev->aclkdiv), ACLKRDIVMASK);
    mcasp_mod_bits( dev->base + DAVINCI_MCASP_ACLKXCTL_REG, ACLKXDIV(dev->aclkdiv), ACLKXDIVMASK);
#endif

	return 0;
}

static void davinci_hw_common_param(struct davinci_audio_dev *dev, int stream)
{
	int i;
	u8 tx_ser = 0;
	u8 rx_ser = 0;

	/* Default configuration */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_PWREMUMGT_REG, MCASP_SOFT);

	/* All PINS as McASP */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_PFUNC_REG, 0x00000000);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mcasp_set_reg(dev->base + DAVINCI_MCASP_TXSTAT_REG, 0xFFFFFFFF);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_XEVTCTL_REG,
				TXDATADMADIS);
	} else {
		mcasp_set_reg(dev->base + DAVINCI_MCASP_RXSTAT_REG, 0xFFFFFFFF);
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_REVTCTL_REG,
				RXDATADMADIS);
	}

	for (i = 0; i < dev->num_serializer; i++) {
		mcasp_set_bits(dev->base + DAVINCI_MCASP_XRSRCTL_REG(i),
					dev->serial_dir[i]);
		if (dev->serial_dir[i] == TX_MODE) {
			mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG,
					AXR(i));
			tx_ser++;
		} else if (dev->serial_dir[i] == RX_MODE) {
			mcasp_clr_bits(dev->base + DAVINCI_MCASP_PDIR_REG,
					AXR(i));
			rx_ser++;
		}
	}

	if (dev->txnumevt && stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (dev->txnumevt * tx_ser > 64)
			dev->txnumevt = 1;

		mcasp_mod_bits(dev->base + DAVINCI_MCASP_WFIFOCTL, tx_ser,
								NUMDMA_MASK);
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_WFIFOCTL,
				((dev->txnumevt * tx_ser) << 8), NUMEVT_MASK);
	}

	if (dev->rxnumevt && stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (dev->rxnumevt * rx_ser > 64)
			dev->rxnumevt = 1;

		mcasp_mod_bits(dev->base + DAVINCI_MCASP_RFIFOCTL, rx_ser,
								NUMDMA_MASK);
		mcasp_mod_bits(dev->base + DAVINCI_MCASP_RFIFOCTL,
				((dev->rxnumevt * rx_ser) << 8), NUMEVT_MASK);
	}
}

static void davinci_hw_param(struct davinci_audio_dev *dev, int stream)
{
	int i, active_slots;
	u32 mask = 0;

	active_slots = (dev->tdm_slots > 31) ? 32 : dev->tdm_slots;
	for (i = 0; i < active_slots; i++)
		mask |= (1 << i);

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
 if ( dev->use_tx_clk_for_rx ) {
                mcasp_clr_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG, TX_ASYNC);
        } else {
                mcasp_set_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG, TX_ASYNC);
        }
#else
	mcasp_clr_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG, TX_ASYNC);
#endif

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* bit stream is MSB first  with no delay */
		/* DSP_B mode */
		mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG,
				AHCLKXE);
		mcasp_set_reg(dev->base + DAVINCI_MCASP_TXTDM_REG, mask);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMT_REG, TXORD);

		if ((dev->tdm_slots >= 2) || (dev->tdm_slots <= 32))
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG,
					FSXMOD(dev->tdm_slots), FSXMOD(0x1FF));
		else
			printk(KERN_ERR "playback tdm slot %d not supported\n",
				dev->tdm_slots);
#if !defined(CONFIG_MACH_Z3_DM816X_MOD) || !defined(CONFIG_MACH_Z3_DM814X_MOD)
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_TXFMCTL_REG, FSXDUR);
#endif
	} else {
		/* bit stream is MSB first with no delay */
		/* DSP_B mode */
		mcasp_set_bits(dev->base + DAVINCI_MCASP_RXFMT_REG, RXORD);
		mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKRCTL_REG,
				AHCLKRE);
		mcasp_set_reg(dev->base + DAVINCI_MCASP_RXTDM_REG, mask);

		if ((dev->tdm_slots >= 2) || (dev->tdm_slots <= 32))
			mcasp_mod_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG,
					FSRMOD(dev->tdm_slots), FSRMOD(0x1FF));
		else
			printk(KERN_ERR "capture tdm slot %d not supported\n",
				dev->tdm_slots);

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
		mcasp_clr_bits(dev->base + DAVINCI_MCASP_RXFMCTL_REG, FSRDUR);
#endif
	}
}

/* S/PDIF */
static void davinci_hw_dit_param(struct davinci_audio_dev *dev)
{
	/* Set the PDIR for Serialiser as output */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_PDIR_REG, AFSX);

	/* TXMASK for 24 bits */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXMASK_REG, 0x00FFFFFF);

	/* Set the TX format : 24 bit right rotation, 32 bit slot, Pad 0
	   and LSB first */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_TXFMT_REG,
						TXROT(6) | TXSSZ(15));

	/* Set TX frame synch : DIT Mode, 1 bit width, internal, rising edge */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXFMCTL_REG,
						AFSXE | FSXMOD(0x180));

	/* Set the TX tdm : for all the slots */
	mcasp_set_reg(dev->base + DAVINCI_MCASP_TXTDM_REG, 0xFFFFFFFF);

	/* Set the TX clock controls : div = 1 and internal */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_ACLKXCTL_REG,
						ACLKXE | TX_ASYNC);

	mcasp_clr_bits(dev->base + DAVINCI_MCASP_XEVTCTL_REG, TXDATADMADIS);

	/* Only 44100 and 48000 are valid, both have the same setting */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_AHCLKXCTL_REG, AHCLKXDIV(3));

	/* Enable the DIT */
	mcasp_set_bits(dev->base + DAVINCI_MCASP_TXDITCTL_REG, DITEN);
}

static int davinci_mcasp_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *cpu_dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	struct davinci_pcm_dma_params *dma_params =
					&dev->dma_params[substream->stream];
	int word_length;
	u8 fifo_level;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    mcasp_set_board_clks(dev);
#endif

	davinci_hw_common_param(dev, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		fifo_level = dev->txnumevt;
	else
		fifo_level = dev->rxnumevt;

	if (dev->op_mode == DAVINCI_MCASP_DIT_MODE)
		davinci_hw_dit_param(dev);
	else
		davinci_hw_param(dev, substream->stream);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		dma_params->data_type = 1;
		word_length = DAVINCI_AUDIO_WORD_8;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		dma_params->data_type = 2;
		word_length = DAVINCI_AUDIO_WORD_16;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		dma_params->data_type = 4;
		word_length = DAVINCI_AUDIO_WORD_32;
		break;

	default:
		printk(KERN_WARNING "davinci-mcasp: unsupported PCM format");
		return -EINVAL;
	}

	if (dev->version == MCASP_VERSION_2 && !fifo_level)
		dma_params->acnt = 4;
	else
		dma_params->acnt = dma_params->data_type;

	dma_params->fifo_level = fifo_level;
	davinci_config_channel_size(dev, word_length);

	return 0;
}

static int davinci_mcasp_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *cpu_dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!dev->clk_active) {
			clk_enable(dev->clk);
			dev->clk_active = 1;
		}
		davinci_mcasp_start(dev, substream->stream);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
		davinci_mcasp_stop(dev, substream->stream);
		if (dev->clk_active) {
			clk_disable(dev->clk);
			dev->clk_active = 0;
		}

		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		davinci_mcasp_stop(dev, substream->stream);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int davinci_mcasp_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct davinci_audio_dev *dev = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_set_dma_data(dai, substream, dev->dma_params);
	return 0;
}

static struct snd_soc_dai_ops davinci_mcasp_dai_ops = {
	.startup	= davinci_mcasp_startup,
	.trigger	= davinci_mcasp_trigger,
	.hw_params	= davinci_mcasp_hw_params,
	.set_fmt	= davinci_mcasp_set_dai_fmt,

};

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
static struct snd_soc_dai_driver davinci_mcasp_dai[] = {
	{
		.name		= "davinci-mcasp.0",
		.playback	= {
			.channels_min	= 2,
			.channels_max 	= 2,
			.rates 		= DAVINCI_MCASP_RATES,
			.formats 	= SNDRV_PCM_FMTBIT_S8 |
						SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture 	= {
			.channels_min 	= 2,
			.channels_max 	= 2,
			.rates 		= DAVINCI_MCASP_RATES,
			.formats	= SNDRV_PCM_FMTBIT_S8 |
						SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops 		= &davinci_mcasp_dai_ops,

	},
	{
		"davinci-mcasp.1",
		.playback 	= {
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= DAVINCI_MCASP_RATES,
			.formats	= SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops 		= &davinci_mcasp_dai_ops,
	},

};
#else

static struct snd_soc_dai_driver davinci_mcasp_dai[DAVINCI_CPU_MAX_MCASP][2] = {
        {
                {
                        .name		= "davinci-mcasp.0",
                        .playback	= {
                                .channels_min	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats 	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .capture 	= {
                                .channels_min 	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,

                },
                {
                        .name		= "davinci-mcasp.0",
                        .playback 	= {
                                .channels_min	= 1,
                                .channels_max	= 384,
                                .rates		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S16_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,
                },
        },
        {
                {
                        .name		= "davinci-mcasp.1",
                        .playback	= {
                                .channels_min	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats 	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .capture 	= {
                                .channels_min 	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,

                },
                {
                        .name		= "davinci-mcasp.1",
                        .playback 	= {
                                .channels_min	= 1,
                                .channels_max	= 384,
                                .rates		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S16_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,
                },
        },
        {
                {
                        .name		= "davinci-mcasp.2",
                        .playback	= {
                                .channels_min	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats 	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .capture 	= {
                                .channels_min 	= 2,
                                .channels_max 	= 2,
                                .rates 		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S8 |
                                SNDRV_PCM_FMTBIT_S16_LE |
                                SNDRV_PCM_FMTBIT_S32_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,

                },
                {
                        .name		= "davinci-mcasp.2",
                        .playback 	= {
                                .channels_min	= 1,
                                .channels_max	= 384,
                                .rates		= DAVINCI_MCASP_RATES,
                                .formats	= SNDRV_PCM_FMTBIT_S16_LE,
                        },
                        .ops 		= &davinci_mcasp_dai_ops,
                },
        },

};

#endif //!machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()

static int davinci_mcasp_probe(struct platform_device *pdev)
{
	struct davinci_pcm_dma_params *dma_data;
	struct resource *mem, *ioarea, *res;
	struct snd_platform_data *pdata;
	struct davinci_audio_dev *dev;
	int ret = 0;

	dev = kzalloc(sizeof(struct davinci_audio_dev), GFP_KERNEL);
	if (!dev)
		return	-ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		ret = -ENODEV;
		goto err_release_data;
	}

	ioarea = request_mem_region(mem->start,
			resource_size(mem), pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "Audio region already claimed\n");
		ret = -EBUSY;
		goto err_release_data;
	}

	pdata = pdev->dev.platform_data;
	dev->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(dev->clk)) {
		ret = -ENODEV;
		goto err_release_region;
	}

	clk_enable(dev->clk);
	dev->clk_active = 1;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    dev->id = pdev->id;	
    mcasp_set_board_clks(dev);	
#endif

	dev->base = ioremap(mem->start, resource_size(mem));
	if (!dev->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release_clk;
	}

	dev->op_mode = pdata->op_mode;
	dev->tdm_slots = pdata->tdm_slots;
	dev->num_serializer = pdata->num_serializer;
	dev->serial_dir = pdata->serial_dir;
	dev->codec_fmt = pdata->codec_fmt;
	dev->version = pdata->version;
	dev->txnumevt = pdata->txnumevt;
	dev->rxnumevt = pdata->rxnumevt;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
    dev->rrot_nibbles_additional = pdata->rrot_nibbles;
#endif

	dma_data = &dev->dma_params[SNDRV_PCM_STREAM_PLAYBACK];
	dma_data->asp_chan_q = pdata->asp_chan_q;
	dma_data->ram_chan_q = pdata->ram_chan_q;
	if (cpu_is_ti81xx())
		dma_data->dma_addr = (dma_addr_t) (pdata->tx_dma_offset);
	else
		dma_data->dma_addr = (dma_addr_t) (pdata->tx_dma_offset
							+ mem->start);

	/* first TX, then RX */
	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "no DMA resource\n");
		ret = -ENODEV;
		goto err_iounmap;
	}

	dma_data->channel = res->start;

	dma_data = &dev->dma_params[SNDRV_PCM_STREAM_CAPTURE];
	dma_data->asp_chan_q = pdata->asp_chan_q;
	dma_data->ram_chan_q = pdata->ram_chan_q;
	if (cpu_is_ti81xx())
		dma_data->dma_addr = (dma_addr_t) (pdata->rx_dma_offset);
	else
		dma_data->dma_addr = (dma_addr_t) (pdata->rx_dma_offset
							+ mem->start);

	res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!res) {
		dev_err(&pdev->dev, "no DMA resource\n");
		ret = -ENODEV;
		goto err_iounmap;
	}

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
if (!pdev->id > DAVINCI_CPU_MAX_MCASP) {
		dev_err(&pdev->dev, "mcasp id out of range (%u>=%u)\n", pdev->id, DAVINCI_CPU_MAX_MCASP);
		ret = -ENODEV;
		goto err_iounmap;
	}
#endif
	dma_data->channel = res->start;
	dev_set_drvdata(&pdev->dev, dev);
	ret = snd_soc_register_dai(&pdev->dev, &davinci_mcasp_dai[pdata->op_mode]);

	if (ret != 0)
		goto err_iounmap;
	return 0;

err_iounmap:
	iounmap(dev->base);
err_release_clk:
	clk_disable(dev->clk);
	clk_put(dev->clk);
err_release_region:
	release_mem_region(mem->start, resource_size(mem));
err_release_data:
	kfree(dev);

	return ret;
}

static int davinci_mcasp_remove(struct platform_device *pdev)
{
	struct davinci_audio_dev *dev = dev_get_drvdata(&pdev->dev);
	struct resource *mem;

	snd_soc_unregister_dai(&pdev->dev);
	clk_disable(dev->clk);
	clk_put(dev->clk);
	dev->clk = NULL;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));

	kfree(dev);

	return 0;
}

static struct platform_driver davinci_mcasp_driver = {
	.probe		= davinci_mcasp_probe,
	.remove		= davinci_mcasp_remove,
	.driver		= {
		.name	= "davinci-mcasp",
		.owner	= THIS_MODULE,
	},
};

static int __init davinci_mcasp_init(void)
{
	return platform_driver_register(&davinci_mcasp_driver);
}
module_init(davinci_mcasp_init);

static void __exit davinci_mcasp_exit(void)
{
	platform_driver_unregister(&davinci_mcasp_driver);
}
module_exit(davinci_mcasp_exit);

MODULE_AUTHOR("Steve Chen");
MODULE_DESCRIPTION("TI DAVINCI McASP SoC Interface");
MODULE_LICENSE("GPL");

