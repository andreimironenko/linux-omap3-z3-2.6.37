/*
 * ASoC driver for TI DAVINCI EVM platform
 *
 * Author:      Vladimir Barinov, <vbarinov@embeddedalley.com>
 * Copyright:   (C) 2007 MontaVista Software, Inc., <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/dma.h>
#include <asm/mach-types.h>

#ifndef CONFIG_ARCH_TI81XX
#include <mach/asp.h>
#include <mach/edma.h>
#include <mach/mux.h>
#else
#include <plat/asp.h>
#include <asm/hardware/edma.h>

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
#include <mach/z3_fpga.h>
#include <mach/z3_app.h>
#include <linux/reboot.h>
#endif

#endif

#include "../codecs/tlv320aic3x.h"
#include "davinci-pcm.h"
#include "davinci-i2s.h"
#include "davinci-mcasp.h"

#define AUDIO_FORMAT (SND_SOC_DAIFMT_DSP_B | \
		SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_IB_NF)
static int evm_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned sysclk;

	/* ASP1 on DM355 EVM is clocked by an external oscillator */
	if (machine_is_davinci_dm355_evm() || machine_is_davinci_dm6467_evm() ||
	    machine_is_davinci_dm365_evm())
		sysclk = 27000000;

	/* ASP0 in DM6446 EVM is clocked by U55, as configured by
	 * board-dm644x-evm.c using GPIOs from U18.  There are six
	 * options; here we "know" we use a 48 KHz sample rate.
	 */
	else if (machine_is_davinci_evm())
		sysclk = 12288000;

	else if (machine_is_davinci_da830_evm() ||
				machine_is_davinci_da850_evm() ||
				machine_is_ti8168evm() ||
				machine_is_ti8148evm() ||
				machine_is_dm385evm())
		sysclk = 24576000;

        else if ( machine_is_ti8148evm() ) {

#if machine_is_z3_814x_mod()
		sysclk = 31250000;
#else
		sysclk = 24576000;
#endif
        }
	else
		return -EINVAL;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, sysclk, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
}

static int evm_sdi_out_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

        unsigned int codec_slave_format = (SND_SOC_DAIFMT_I2S |      
                                           SND_SOC_DAIFMT_CBS_CFS | 
                                           SND_SOC_DAIFMT_NB_NF);

	int ret = 0;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, codec_slave_format);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, codec_slave_format);
	if (ret < 0)
		return ret;

	return 0;
}

static int evm_sdi_in_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

        unsigned int codec_slave_format = (SND_SOC_DAIFMT_RIGHT_J |      
                                           SND_SOC_DAIFMT_CBM_CFM | 
                                           SND_SOC_DAIFMT_NB_NF);

	int ret = 0;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, codec_slave_format);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, codec_slave_format);
	if (ret < 0)
		return ret;

	return 0;
}

static int evm_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	/* set cpu DAI configuration */
	return snd_soc_dai_set_fmt(cpu_dai, AUDIO_FORMAT);
}

static int evm_ext_master_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
        int ret=0;
        unsigned int ext_master_format = (SND_SOC_DAIFMT_I2S |      
                                          SND_SOC_DAIFMT_CBM_CFM | 
                                          SND_SOC_DAIFMT_NB_NF);


	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, ext_master_format);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret= snd_soc_dai_set_fmt(cpu_dai, ext_master_format);

        return ret;
}

static int evm_codec_slave_ext_master_hw_params(struct snd_pcm_substream *substream,
                                        struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
        int ret=0;
        unsigned int codec_format = (SND_SOC_DAIFMT_I2S |      
                                     SND_SOC_DAIFMT_CBS_CFS | 
                                     SND_SOC_DAIFMT_NB_NF);
        unsigned int ext_master_format = (SND_SOC_DAIFMT_I2S |      
                                          SND_SOC_DAIFMT_CBM_CFM | 
                                          SND_SOC_DAIFMT_NB_NF);

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, codec_format);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret= snd_soc_dai_set_fmt(cpu_dai, ext_master_format);

        return ret;
}

static struct snd_soc_ops evm_ops = {
	.hw_params = evm_hw_params,
};

static struct snd_soc_ops evm_sdi_out_ops = {
	.hw_params = evm_sdi_out_hw_params,
};

static struct snd_soc_ops evm_sdi_in_ops = {
	.hw_params = evm_sdi_in_hw_params,
};

static struct snd_soc_ops evm_spdif_ops = {
	.hw_params = evm_spdif_hw_params,
};

static struct snd_soc_ops evm_ext_master_ops = {
	.hw_params = evm_ext_master_hw_params,
};

static struct snd_soc_ops evm_switch_ops = {
	.hw_params = evm_hw_params, /* may be changed at init time */ 
};

/* davinci-evm machine dapm widgets */
static const struct snd_soc_dapm_widget aic3x_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
};

/* davinci-evm machine audio_mapnections to the codec pins */
static const struct snd_soc_dapm_route audio_map[] = {
	/* Headphone connected to HPLOUT, HPROUT */
	{"Headphone Jack", NULL, "HPLOUT"},
	{"Headphone Jack", NULL, "HPROUT"},

	/* Line Out connected to LLOUT, RLOUT */
	{"Line Out", NULL, "LLOUT"},
	{"Line Out", NULL, "RLOUT"},

	/* Mic connected to (MIC3L | MIC3R) */
	{"MIC3L", NULL, "Mic Bias 2V"},
	{"MIC3R", NULL, "Mic Bias 2V"},
	{"Mic Bias 2V", NULL, "Mic Jack"},

	/* Line In connected to (LINE1L | LINE2L), (LINE1R | LINE2R) */
	{"LINE1L", NULL, "Line In"},
	{"LINE2L", NULL, "Line In"},
	{"LINE1R", NULL, "Line In"},
	{"LINE2R", NULL, "Line In"},
};

/* Logic for a aic3x as connected on a davinci-evm */
static int evm_aic3x_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;

	/* Add davinci-evm specific widgets */
	snd_soc_dapm_new_controls(codec, aic3x_dapm_widgets,
				  ARRAY_SIZE(aic3x_dapm_widgets));

	/* Set up davinci-evm specific audio path audio_map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	/* not connected */
	snd_soc_dapm_disable_pin(codec, "MONO_LOUT");
	snd_soc_dapm_disable_pin(codec, "HPLCOM");
	snd_soc_dapm_disable_pin(codec, "HPRCOM");

	/* always connected */
	snd_soc_dapm_enable_pin(codec, "Headphone Jack");
	snd_soc_dapm_enable_pin(codec, "Line Out");
	snd_soc_dapm_enable_pin(codec, "Mic Jack");
	snd_soc_dapm_enable_pin(codec, "Line In");

	snd_soc_dapm_sync(codec);

	return 0;
}

static int evm_aic3x_2nd_init(struct snd_soc_pcm_runtime *rtd)
{
        return 0;
}

/* Logic for a adv7611 aic3x as connected on a davinci-evm */
static int evm_adv7611_init(struct snd_soc_pcm_runtime *rtd)
{
        return 0;
}

/* Logic for a gv7601 aic3x as connected on a davinci-evm */
static int evm_gv7601_init(struct snd_soc_pcm_runtime *rtd)
{
        return 0;
}

/* Logic for a gv7600 as connected on a davinci-evm */
static int evm_gv7600_init(struct snd_soc_pcm_runtime *rtd)
{
        return 0;
}

/* davinci-evm digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link dm6446_evm_dai = {
	.name = "TLV320AIC3X",
	.stream_name = "AIC3X",
	.cpu_dai_name = "davinci-mcbsp",
	.codec_dai_name = "tlv320aic3x-hifi",
	.codec_name = "tlv320aic3x-codec.1-001b",
	.platform_name = "davinci-pcm-audio",
	.init = evm_aic3x_init,
	.ops = &evm_ops,
};

static struct snd_soc_dai_link dm355_evm_dai = {
	.name = "TLV320AIC3X",
	.stream_name = "AIC3X",
	.cpu_dai_name = "davinci-mcbsp.1",
	.codec_dai_name = "tlv320aic3x-hifi",
	.codec_name = "tlv320aic3x-codec.1-001b",
	.platform_name = "davinci-pcm-audio",
	.init = evm_aic3x_init,
	.ops = &evm_ops,
};

static struct snd_soc_dai_link dm365_evm_dai = {
#ifdef CONFIG_SND_DM365_AIC3X_CODEC
	.name = "TLV320AIC3X",
	.stream_name = "AIC3X",
	.cpu_dai_name = "davinci-mcbsp",
	.codec_dai_name = "tlv320aic3x-hifi",
	.init = evm_aic3x_init,
	.codec_name = "tlv320aic3x-codec.1-0018",
	.ops = &evm_ops,
#elif defined(CONFIG_SND_DM365_VOICE_CODEC)
	.name = "Voice Codec - CQ93VC",
	.stream_name = "CQ93",
	.cpu_dai_name = "davinci-vcif",
	.codec_dai_name = "cq93vc-hifi",
	.codec_name = "cq93vc-codec",
#endif
	.platform_name = "davinci-pcm-audio",
};

static struct snd_soc_dai_link dm6467_evm_dai[] = {
	{
		.name = "TLV320AIC3X",
		.stream_name = "AIC3X",
		.cpu_dai_name= "davinci-mcasp.0",
		.codec_dai_name = "tlv320aic3x-hifi",
		.platform_name ="davinci-pcm-audio",
		.codec_name = "tlv320aic3x-codec.0-001a",
		.init = evm_aic3x_init,
		.ops = &evm_ops,
	},
	{
		.name = "McASP",
		.stream_name = "spdif",
		.cpu_dai_name= "davinci-mcasp.1",
		.codec_dai_name = "dit-hifi",
		.codec_name = "spdif_dit",
		.platform_name = "davinci-pcm-audio",
		.ops = &evm_spdif_ops,
	},
};
static struct snd_soc_dai_link da8xx_evm_dai = {
	.name = "TLV320AIC3X",
	.stream_name = "AIC3X",
	.cpu_dai_name= "davinci-mcasp.0",
	.codec_dai_name = "tlv320aic3x-hifi",
	.codec_name = "tlv320aic3x-codec.0-001a",
	.platform_name = "davinci-pcm-audio",
	.init = evm_aic3x_init,
	.ops = &evm_ops,
};


static struct snd_soc_dai_link ti81xx_evm_dai[] = {
	{
		.name = "TLV320AIC3X",
		.stream_name = "AIC3X",
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
        .cpu_dai_name= "davinci-mcasp.2",
#endif
		.codec_dai_name = "tlv320aic3x-hifi",
		.codec_name = "tlv320aic3x-codec.1-0018",
		.platform_name = "davinci-pcm-audio",
		.init = evm_aic3x_init,
#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
		.ops = &evm_ops,
#else
        .ops = &evm_switch_ops, // may be master or slave
#endif
	},
	
#ifdef CONFIG_SND_SOC_TI81XX_HDMI
	{
		.name = "HDMI_SOC_LINK",
		.stream_name = "hdmi",
		.cpu_dai_name = "hdmi-dai",
		.platform_name = "davinci-pcm-audio",
		.codec_dai_name = "HDMI-DAI-CODEC",     /* DAI name */
		.codec_name = "hdmi-dummy-codec",
	},
#endif
};
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
static struct snd_soc_dai_link z3_dm81xx_app_02_dai[] = {
        {
                .name = "TLV320AIC3X_2",
                .stream_name = "AIC3X_2",
                .cpu_dai_name= "davinci-mcasp.1",
                .codec_dai_name = "tlv320aic3x-hifi",
                .codec_name = "tlv320aic3x-codec.2-0018",
                .platform_name = "davinci-pcm-audio",
                .init = evm_aic3x_2nd_init,
                .ops = &evm_ops,
        },
#if (defined(CONFIG_VIDEO_ADV7611) || defined(CONFIG_VIDEO_ADV7611_MODULE))
        {
                .name = "ADV7611_SOUND",
                .stream_name = "ADV7611",
                .cpu_dai_name= "davinci-mcasp.0",
                .codec_dai_name = "adv7611-hifi",
                .codec_name = "adv7611-sound.1",
                .platform_name = "davinci-pcm-audio",
                .init = evm_adv7611_init,
                .ops = &evm_ext_master_ops,
        },
        {
                .name = "ADV7611_SOUND_2",
                .stream_name = "ADV7611_2",
                .cpu_dai_name= "davinci-mcasp.1",
                .codec_dai_name = "adv7611-hifi",
                .codec_name = "adv7611-sound.2",
                .platform_name = "davinci-pcm-audio",
                .init = evm_adv7611_init,
                .ops = &evm_ext_master_ops,
        },
#endif
};

static struct snd_soc_dai_link z3_dm81xx_app_32_dai[] = {
        {
                .name = "ADV7611_SOUND",
                .stream_name = "ADV7611",
                .cpu_dai_name= "davinci-mcasp.2",
                .codec_dai_name = "adv7611-hifi",
                .codec_name = "adv7611-sound.1", // distinguish by I2C address?
                .platform_name = "davinci-pcm-audio",
                .init = evm_adv7611_init,
                .ops = &evm_ext_master_ops,
        },
        {
                .name = "GV7601_SOUND",
                .stream_name = "GV7601",
                .cpu_dai_name= "davinci-mcasp.0",
                .codec_dai_name = "gv7601-hifi",
                .codec_name = "gv7601-sound", // distinguish by I2C address?
                .platform_name = "davinci-pcm-audio",
                .init = evm_gv7601_init,
                .ops = &evm_sdi_in_ops,
        },
        {
                .name = "GV7600_SOUND",
                .stream_name = "GV7600",
                .cpu_dai_name= "davinci-mcasp.1",
                .codec_dai_name = "gv7600-hifi",
                .codec_name = "gv7600-sound", // distinguish by I2C address?
                .platform_name = "davinci-pcm-audio",
                .init = evm_gv7600_init,
                .ops = &evm_sdi_out_ops,
        },
};

static struct snd_soc_dai_link z3_dm81xx_app_22_dai[] = {
        {
                .name = "ADV7611_SOUND",
                .stream_name = "ADV7611",
                .cpu_dai_name= "davinci-mcasp.0",
                .codec_dai_name = "adv7611-hifi",
                .codec_name = "adv7611-sound.1",
                .platform_name = "davinci-pcm-audio",
                .init = evm_adv7611_init,
                .ops = &evm_ext_master_ops,
        },
};

#endif //defined(CONFIG_MACH_Z3_DM816X_MOD) || defined(CONFIG_MACH_Z3_DM814X_MOD)

/* davinci dm6446 evm audio machine driver */
static struct snd_soc_card dm6446_snd_soc_card_evm = {
	.name = "DaVinci DM6446 EVM",
	.dai_link = &dm6446_evm_dai,
	.num_links = 1,
};

/* davinci dm355 evm audio machine driver */
static struct snd_soc_card dm355_snd_soc_card_evm = {
	.name = "DaVinci DM355 EVM",
	.dai_link = &dm355_evm_dai,
	.num_links = 1,
};

/* davinci dm365 evm audio machine driver */
static struct snd_soc_card dm365_snd_soc_card_evm = {
	.name = "DaVinci DM365 EVM",
	.dai_link = &dm365_evm_dai,
	.num_links = 1,
};

/* davinci dm6467 evm audio machine driver */
static struct snd_soc_card dm6467_snd_soc_card_evm = {
	.name = "DaVinci DM6467 EVM",
	.dai_link = dm6467_evm_dai,
	.num_links = ARRAY_SIZE(dm6467_evm_dai),
};

static struct snd_soc_card da830_snd_soc_card = {
	.name = "DA830/OMAP-L137 EVM",
	.dai_link = &da8xx_evm_dai,
	.num_links = 1,
};

static struct snd_soc_card da850_snd_soc_card = {
	.name = "DA850/OMAP-L138 EVM",
	.dai_link = &da8xx_evm_dai,
	.num_links = 1,
};

static struct snd_soc_card ti81xx_snd_soc_card = {
	.name = "TI81XX EVM",
	.dai_link = ti81xx_evm_dai,
	.num_links = ARRAY_SIZE(ti81xx_evm_dai),
};

#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
static struct snd_soc_card z3_dm81xx_app_02_snd_soc_card = {
        .name = "APP02",
        .dai_link = z3_dm81xx_app_02_dai,
        .num_links = ARRAY_SIZE(z3_dm81xx_app_02_dai),
};

static struct snd_soc_card z3_dm81xx_app_32_snd_soc_card = {
        .name = "APP32",
        .dai_link = z3_dm81xx_app_32_dai,
        .num_links = ARRAY_SIZE(z3_dm81xx_app_32_dai),
};

static struct snd_soc_card z3_dm81xx_app_22_snd_soc_card = {
        .name = "APP22",
        .dai_link = z3_dm81xx_app_22_dai,
        .num_links = ARRAY_SIZE(z3_dm81xx_app_22_dai),
};
#endif //defined(CONFIG_MACH_Z3_DM816X_MOD) || defined(CONFIG_MACH_Z3_DM814X_MOD)

static void ti81xx_evm_dai_fixup(void)
{
#if !machine_is_z3_816x_mod() && !machine_is_z3_814x_mod()
	if (machine_is_ti8168evm() || machine_is_ti8148evm()) {
		ti81xx_evm_dai[0].cpu_dai_name = "davinci-mcasp.2";
	} else if (machine_is_dm385evm()) {
		ti81xx_evm_dai[0].cpu_dai_name = "davinci-mcasp.1";
	} else {
		ti81xx_evm_dai[0].cpu_dai_name = NULL;
	}
#else
 int i;

	if (machine_is_ti8168evm()) {
        } else if (machine_is_ti8148evm()) {
                for (i=0; i<ARRAY_SIZE(z3_dm81xx_app_02_dai); i++ ) {
                        if (!strcmp( z3_dm81xx_app_02_dai[i].name, "TLV320AIC3X_2" ) ) {
                                z3_dm81xx_app_02_dai[i].codec_name = "tlv320aic3x-codec.3-0018";
                        }
                }
	} else if (machine_is_dm385evm()) {
		ti81xx_evm_dai[0].cpu_dai_name = "davinci-mcasp.1";
//	} else {
//		ti81xx_evm_dai[0].cpu_dai_name = NULL;
	}
#endif	//!defined(CONFIG_MACH_Z3_DM816X_MOD) || !defined(CONFIG_MACH_Z3_DM814X_MOD)
	
}

static struct platform_device *evm_snd_device;
static int __init evm_init(void)
{
	struct snd_soc_card *evm_snd_dev_data;
	int index;
	int ret;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()

#ifdef CONFIG_ARCH_TI81XX
        int board_id = z3_fpga_board_id() ;
#endif

    printk( KERN_DEBUG "ALSA init: board_id=%u\n", board_id );
#endif //defined(CONFIG_MACH_Z3_DM816X_MOD) || defined(CONFIG_MACH_Z3_DM814X_MOD)

	if (machine_is_davinci_evm()) {
		evm_snd_dev_data = &dm6446_snd_soc_card_evm;
		index = 0;
	} else if (machine_is_davinci_dm355_evm()) {
		evm_snd_dev_data = &dm355_snd_soc_card_evm;
		index = 1;
	} else if (machine_is_davinci_dm365_evm()) {
		evm_snd_dev_data = &dm365_snd_soc_card_evm;
		index = 0;
	} else if (machine_is_davinci_dm6467_evm()) {
		evm_snd_dev_data = &dm6467_snd_soc_card_evm;
		index = 0;
	} else if (machine_is_davinci_da830_evm()) {
		evm_snd_dev_data = &da830_snd_soc_card;
		index = 1;
	} else if (machine_is_davinci_da850_evm()) {
		evm_snd_dev_data = &da850_snd_soc_card;
		index = 0;
	} else if (machine_is_ti8168evm() || machine_is_ti8148evm()
					|| machine_is_dm385evm()) {
		ti81xx_evm_dai_fixup();
		evm_snd_dev_data = &ti81xx_snd_soc_card;
		index = 0;
#if machine_is_z3_816x_mod() || machine_is_z3_814x_mod()
switch ( board_id )  {
                case Z3_BOARD_ID_APP_31:
                        /* Slave on-board PCM when sample rate generator is supported */
                        evm_switch_ops.hw_params = evm_codec_slave_ext_master_hw_params;
                        z3_fpga_set_aic_ext_master(1);
                        break;
                default:
                        break;
                                
                }
#endif //defined(CONFIG_MACH_Z3_DM816X_MOD) || defined(CONFIG_MACH_Z3_DM814X_MOD)

	} else
		return -EINVAL;

	evm_snd_device = platform_device_alloc("soc-audio", index);
	if (!evm_snd_device)
		return -ENOMEM;

	platform_set_drvdata(evm_snd_device, evm_snd_dev_data);
	ret = platform_device_add(evm_snd_device);
	if (ret)
		platform_device_put(evm_snd_device);

 if (machine_is_z3_816x_mod() || machine_is_z3_814x_mod()) {
#ifdef CONFIG_ARCH_TI81XX
                evm_snd_dev_data = NULL;
                
                switch ( board_id )  {
                case Z3_BOARD_ID_APP_02:
                        evm_snd_dev_data = &z3_dm81xx_app_02_snd_soc_card;
                        break;
                case Z3_BOARD_ID_APP_21:
                        evm_snd_dev_data = &z3_dm81xx_app_22_snd_soc_card;
                        break;
                case Z3_BOARD_ID_APP_31:
                        evm_snd_dev_data = &z3_dm81xx_app_32_snd_soc_card;
                        break;
                default:
                        break;
                                
                }

                if ( NULL != evm_snd_dev_data ) {
                        ++index;
                        evm_snd_device = platform_device_alloc("soc-audio", index);
                        if (!evm_snd_device)
                                return -ENOMEM;
                        
                        platform_set_drvdata(evm_snd_device, evm_snd_dev_data);
                        ret = platform_device_add(evm_snd_device);
                        if (ret)
                                platform_device_put(evm_snd_device);
                        
                }
	}
#endif
	return ret;
}

static void __exit evm_exit(void)
{
	platform_device_unregister(evm_snd_device);
}

module_init(evm_init);
module_exit(evm_exit);

MODULE_AUTHOR("Vladimir Barinov");
MODULE_DESCRIPTION("TI DAVINCI EVM ASoC driver");
MODULE_LICENSE("GPL");
