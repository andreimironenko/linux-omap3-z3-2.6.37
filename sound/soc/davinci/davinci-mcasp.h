/*
 * ALSA SoC McASP Audio Layer for TI DAVINCI processor
 *
 * MCASP related definitions
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

#ifndef DAVINCI_MCASP_H
#define DAVINCI_MCASP_H

#include <linux/io.h>

#ifndef CONFIG_ARCH_TI81XX
#include <mach/asp.h>
#else
#include <plat/asp.h>
#endif

#include <asm/mach-types.h>

#include "davinci-pcm.h"

#define DAVINCI_MCASP_RATES	SNDRV_PCM_RATE_8000_96000
#define DAVINCI_MCASP_I2S_DAI	0
#define DAVINCI_MCASP_DIT_DAI	1

#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
enum {
	DAVINCI_AUDIO_WORD_8 = 0,
	DAVINCI_AUDIO_WORD_12,
	DAVINCI_AUDIO_WORD_16,
	DAVINCI_AUDIO_WORD_20,
	DAVINCI_AUDIO_WORD_24,
	DAVINCI_AUDIO_WORD_32,
	DAVINCI_AUDIO_WORD_28,  /* This is only valid for McASP */
};
#else
typedef enum {
	DAVINCI_AUDIO_WORD_8 = 0,
	DAVINCI_AUDIO_WORD_12,
	DAVINCI_AUDIO_WORD_16,
	DAVINCI_AUDIO_WORD_20,
	DAVINCI_AUDIO_WORD_24,
	DAVINCI_AUDIO_WORD_32,
	DAVINCI_AUDIO_WORD_28,  /* This is only valid for McASP */
} davinci_mcasp_audio_word_t;
#endif //!defined(CONFIG_MACH_Z3_DM816X_MOD) || !defined(CONFIG_MACH_Z3_DM814X_MOD)

struct davinci_audio_dev {
	struct davinci_pcm_dma_params dma_params[2];
	void __iomem *base;
	int sample_rate;
	struct clk *clk;
	unsigned int codec_fmt;
	u8 clk_active;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        u8          aclkdiv;
        u32         pdir_tx_mask;

        /* Hardware options */
        u32 mclk_out; // Bit mask of HCLK pins to drive master clock for codec 
        u32 use_tx_clk_for_rx; 
        int id;
		u8      rrot_nibbles_additional;
#endif
	/* McASP specific data */
	int	tdm_slots;
	u8	op_mode;
	u8	num_serializer;
	u8	*serial_dir;
	u8	version;

	/* McASP FIFO related */
	u8	txnumevt;
	u8	rxnumevt;
};

#endif	/* DAVINCI_MCASP_H */
