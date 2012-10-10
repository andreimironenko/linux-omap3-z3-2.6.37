/*
 * ALSA SoC ADV7611 HDMI receiver audio driver
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

#define ADV7611_SOUND_RATES	SNDRV_PCM_RATE_8000_96000
#define ADV7611_SOUND_FORMATS	SNDRV_PCM_FMTBIT_S32_LE


static struct snd_soc_codec_driver soc_codec_adv7611_sound;


static int adv7611_sound_hw_params(struct snd_pcm_substream *substream,
                                   struct snd_pcm_hw_params *params,
                                   struct snd_soc_dai *dai)
{
        /* This is where we need to drive the digital bus, 
           when we are sharing the bus with another codec
         */

        z3_fpga_set_aic_disable( 1 );

        return 0;
}

static int adv7611_sound_set_dai_fmt(struct snd_soc_dai *codec_dai,
                                     unsigned int fmt)
{
        return 0;
}

static struct snd_soc_dai_ops adv7611_sound_dai_ops = {
        .hw_params = adv7611_sound_hw_params,
	/* .digital_mute	= adv7611_sound_mute, */
	/* .set_sysclk	= adv7611_sound_set_dai_sysclk, */
	.set_fmt	= adv7611_sound_set_dai_fmt,
};


static struct snd_soc_dai_driver adv7611_sound_dai = {
	.name		= "adv7611-hifi",
	.capture 	= {
		.stream_name	= "Capture",
		.channels_min	= 2,
		.channels_max	= 8,
		.rates		= ADV7611_SOUND_RATES,
		.formats	= ADV7611_SOUND_FORMATS,
	},
	.ops = &adv7611_sound_dai_ops,
};

static int adv7611_sound_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_adv7611_sound,
			&adv7611_sound_dai, 1);
}

static int adv7611_sound_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver adv7611_sound_driver = {
	.probe		= adv7611_sound_probe,
	.remove		= adv7611_sound_remove,
	.driver		= {
		.name	= "adv7611-sound",
		.owner	= THIS_MODULE,
	},
};

static int __init adv7611_sound_modinit(void)
{
	return platform_driver_register(&adv7611_sound_driver);
}

static void __exit adv7611_sound_exit(void)
{
	platform_driver_unregister(&adv7611_sound_driver);
}

module_init(adv7611_sound_modinit);
module_exit(adv7611_sound_exit);

