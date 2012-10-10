/*
 * ALSA SoC GV7601 SDI receiver audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <mach/z3_fpga.h>

MODULE_LICENSE("GPL");

#define GV7601_SOUND_RATES	SNDRV_PCM_RATE_8000_96000
#define GV7601_SOUND_FORMATS	SNDRV_PCM_FMTBIT_S32_LE


static struct snd_soc_codec_driver soc_codec_gv7601_sound;


static int gv7601_sound_hw_params(struct snd_pcm_substream *substream,
                                   struct snd_pcm_hw_params *params,
                                   struct snd_soc_dai *dai)
{
        /* This is where we need to drive the digital bus, 
           when we are sharing the bus with another codec
         */

        return 0;
}

static int gv7601_sound_set_dai_fmt(struct snd_soc_dai *codec_dai,
                                     unsigned int fmt)
{
        return 0;
}

static struct snd_soc_dai_ops gv7601_sound_dai_ops = {
        .hw_params = gv7601_sound_hw_params,
	/* .digital_mute	= gv7601_sound_mute, */
	/* .set_sysclk	= gv7601_sound_set_dai_sysclk, */
	.set_fmt	= gv7601_sound_set_dai_fmt,
};


static struct snd_soc_dai_driver gv7601_sound_dai = {
	.name		= "gv7601-hifi",
	.capture 	= {
		.stream_name	= "Capture",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= GV7601_SOUND_RATES,
		.formats	= GV7601_SOUND_FORMATS,
	},
	.ops = &gv7601_sound_dai_ops,
};

static int gv7601_sound_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_gv7601_sound,
			&gv7601_sound_dai, 1);
}

static int gv7601_sound_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver gv7601_sound_driver = {
	.probe		= gv7601_sound_probe,
	.remove		= gv7601_sound_remove,
	.driver		= {
		.name	= "gv7601-sound",
		.owner	= THIS_MODULE,
	},
};

static int __init gv7601_sound_modinit(void)
{ 
        int ret;
        
	ret = platform_driver_register(&gv7601_sound_driver);

        return ret;
}

static void __exit gv7601_sound_exit(void)
{
	platform_driver_unregister(&gv7601_sound_driver);
}

module_init(gv7601_sound_modinit);
module_exit(gv7601_sound_exit);

