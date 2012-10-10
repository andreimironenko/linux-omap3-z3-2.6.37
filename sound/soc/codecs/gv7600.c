/*
 * ALSA SoC GV7600 SDI transmitter audio driver
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

#define GV7600_SOUND_RATES	SNDRV_PCM_RATE_8000_96000
#define GV7600_SOUND_FORMATS	SNDRV_PCM_FMTBIT_S32_LE


static struct snd_soc_codec_driver soc_codec_gv7600_sound;


static int gv7600_sound_hw_params(struct snd_pcm_substream *substream,
                                   struct snd_pcm_hw_params *params,
                                   struct snd_soc_dai *dai)
{
        /* This is where we need to drive the digital bus, 
           when we are sharing the bus with another codec
         */

        return 0;
}

static int gv7600_sound_set_dai_fmt(struct snd_soc_dai *codec_dai,
                                     unsigned int fmt)
{
        return 0;
}

static struct snd_soc_dai_ops gv7600_sound_dai_ops = {
        .hw_params = gv7600_sound_hw_params,
	/* .digital_mute	= gv7600_sound_mute, */
	/* .set_sysclk	= gv7600_sound_set_dai_sysclk, */
	.set_fmt	= gv7600_sound_set_dai_fmt,
};


static struct snd_soc_dai_driver gv7600_sound_dai = {
	.name		= "gv7600-hifi",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 4,
		.rates		= GV7600_SOUND_RATES,
		.formats	= GV7600_SOUND_FORMATS,
	},
	.ops = &gv7600_sound_dai_ops,
};

static int gv7600_sound_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_gv7600_sound,
			&gv7600_sound_dai, 1);
}

static int gv7600_sound_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver gv7600_sound_driver = {
	.probe		= gv7600_sound_probe,
	.remove		= gv7600_sound_remove,
	.driver		= {
		.name	= "gv7600-sound",
		.owner	= THIS_MODULE,
	},
};

static int __init gv7600_sound_modinit(void)
{
	return platform_driver_register(&gv7600_sound_driver);
}

static void __exit gv7600_sound_exit(void)
{
	platform_driver_unregister(&gv7600_sound_driver);
}

module_init(gv7600_sound_modinit);
module_exit(gv7600_sound_exit);

