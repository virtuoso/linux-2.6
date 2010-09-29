/*
 * smdk6410_wm8731.c
 *
 * Copyright (C) 2009, iriver. Ltd. 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>

#include <plat/regs-iis.h>
#include <plat/map-base.h>
#include <asm/gpio.h> 
#include <plat/gpio-cfg.h> 
#include <plat/regs-gpio.h>

#include <mach/hardware.h>
#include <mach/audio.h>
#include <asm/io.h>
#include <plat/regs-clock.h>

#include "../codecs/wm8731.h"
#include "s3c-pcm.h"

#include "s3c6410-i2s.h"

#if 0
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

#define SRC_CLK	s3c6410_i2s_get_clockrate()

/* XXX BLC(bits-per-channel) --> BFS(bit clock shud be >= FS*(Bit-per-channel)*2) XXX */
/* XXX BFS --> RFS(must be a multiple of BFS)                                 XXX */
/* XXX RFS & SRC_CLK --> Prescalar Value(SRC_CLK / RFS_VAL / fs - 1)          XXX */
#if 1
static int smdk6410_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	unsigned int clk = 0;
	int bfs, rfs, psr, ret = 0;

	 /* WM8580 codec supports only S16_LE, S20_3LE, S24_LE & S32_LE.
	 * S3C6410 AP supports only S8, S16_LE & S24_LE.
	 * We implement all for completeness but only S16_LE & S24_LE bit-lengths 
	 * are possible for this AP-Codec combination.
	 */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		bfs = 16;
		rfs = 256;		/* Can take any RFS value for AP */
 		break;
 	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		rfs = 256;		/* Can take any RFS value for AP */
 		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
 	case SNDRV_PCM_FORMAT_S24_LE:
		bfs = 48;
		rfs = 512;		/* See Table 41-1,2 of S5P6440 UserManual */
 		break;
	default:
		bfs = 32;
		rfs = 256;		/* Can take any RFS value for AP */
		break;
	//	return -EINVAL;
 	}

		bfs = 32;
		rfs = 256;		/* Can take any RFS value for AP */
 	
 	s3cdbg("%s, %d, format= %d ++++++++ \n", __FUNCTION__, __LINE__, params_format(params));

 	/* Select the Sysclk */
 	#if 0		//janged_clk
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C6410_CDCLKSRC_INT, params_rate(params), SND_SOC_CLOCK_OUT);
	if (ret < 0)
	{
		s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
		//return ret;
	}
	#endif

	#ifdef USE_CLKAUDIO
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C6410_CLKSRC_CLKAUDIO, params_rate(params), SND_SOC_CLOCK_OUT);
	#else
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C6410_CLKSRC_PCLK, 0, SND_SOC_CLOCK_OUT);
	#endif
	if (ret < 0)
	{
		s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
		return ret;
	}

	/* Set the DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
	{
		s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
		return ret;
	}

	if((bfs != 0) && (rfs != 0))
	{
		/* Set the RFS */
		ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C64XX_DIV_MCLK, rfs);
		if (ret < 0)
		{
			s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
			return ret;
		}
		/* Set the BFS */
		ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C64XX_DIV_BCLK, bfs);
		if (ret < 0)
		{
			s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
			return ret;
		}

		
	}

	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 24000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		#if 1
		clk = 12288000;
		psr = SRC_CLK / rfs / params_rate(params);
		ret = SRC_CLK / rfs - psr * params_rate(params);
		if(ret >= params_rate(params)/2)	// round off
		   psr += 1;
		psr -= 1;
		break;
		#endif
	case 11025:
	case 22050:
	case 44100: 
	case 88200:
		clk = 11289600;
		//clk = 12288000;
		psr = SRC_CLK / rfs / params_rate(params);
		ret = SRC_CLK / rfs - psr * params_rate(params);
		if(ret >= params_rate(params)/2)	// round off
		   psr += 1;
		psr -= 1;
		break;
	default:
		clk = 12288000;
		psr = SRC_CLK / rfs / params_rate(params);
		ret = SRC_CLK / rfs - psr * params_rate(params);
		if(ret >= params_rate(params)/2)	// round off
		   psr += 1;
		psr -= 1;
		break;
	
//		return -EINVAL;
	}
	//	printk("[[[ SRC = %d +++++++++++++]]]\n ", SRC_CLK);
	//	printk("[[[ PSR = %d +++++++++++++]]]\n ", psr);
	//	printk("[[[ RET = %d +++++++++++++]]]\n ", ret);
	//	printk("[[[ rate = %d +++++++++++++]]]\n ", params_rate(params));
	/* Set the Prescalar */
	if(clk != 0)
	{
		ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C64XX_DIV_PRESCALER, psr);
		if (ret < 0)
		{
			s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
			return ret;
		}

		/* set codec DAI configuration */
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |	SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFM);
		if (ret < 0)
		{
			s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
			return ret;
		}
		/* set the codec system clock for DAC and ADC */
		ret = snd_soc_dai_set_sysclk(codec_dai, WM8731_SYSCLK, clk, SND_SOC_CLOCK_IN);
		if (ret < 0)
		{
			s3cdbg("F%s, L%d \n", __FUNCTION__, __LINE__);
			return ret;
		}
	}

	return 0;
}
#else
static int smdk6410_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int pll_out = 0;//, bclk = 0;
	int ret = 0;
	unsigned int iispsr, iismod;
	unsigned int prescaler = 4;

	u32*	regs;
	regs = ioremap(S3C64XX_PA_IIS, 0x100);

	s3cdbg("Entered %s, rate = %d\n", __FUNCTION__, params_rate(params));

	/*PCLK & SCLK gating enable*/
	writel(readl(S3C_PCLK_GATE)|S3C_CLKCON_PCLK_IIS0, S3C_PCLK_GATE);
	writel(readl(S3C_SCLK_GATE)|S3C_CLKCON_SCLK_AUDIO0, S3C_SCLK_GATE);

	iismod = readl(regs + S3C64XX_IISMOD);
	iismod &= ~S3C64XX_IISMOD_RFSMASK;
	iismod &= ~S3C64XX_IISMOD_BFSMASK;

	/*Clear I2S prescaler value [13:8] and disable prescaler*/
	iispsr = readl(regs + S3C64XX_IISPSR);	
	iispsr &=~((0x3f<<8)|(1<<15)); 
	writel(iispsr, regs + S3C64XX_IISPSR);
	
	s3cdbg("%s: %d , params = %d\n", __FUNCTION__, __LINE__, params_rate(params));

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 32000:
	case 64100:
		writel(50332, S3C_EPLL_CON1);
		writel((1<<31)|(32<<16)|(1<<8)|(3<<0) ,S3C_EPLL_CON0);
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		writel(10398, S3C_EPLL_CON1);
		writel((1<<31)|(45<<16)|(1<<8)|(3<<0) ,S3C_EPLL_CON0);
		break;
	case 48000:
	case 96000:
		writel(9961, S3C_EPLL_CON1);
		writel((1<<31)|(49<<16)|(1<<8)|(3<<0) ,S3C_EPLL_CON0);
		break;
	default:
		writel(0, S3C_EPLL_CON1);
		writel((1<<31)|(128<<16)|(25<<8)|(0<<0) ,S3C_EPLL_CON0);
		break;
	}

	//s3cdbg("%s, IISCON: %x IISMOD: %x,IISFIC: %x,IISPSR: %x\n",
	//		__FUNCTION__ , readl(S3C64XX_IISCON), readl(S3C64XX_IISMOD), 
	//		readl(S3C64XX_IISMOD), readl(S3C64XX_IISPSR));
	
	while(!(__raw_readl(S3C_EPLL_CON0)&(1<<30)));

	/* MUXepll : FOUTepll */
	writel(readl(S3C_CLK_SRC)|S3C_CLKSRC_EPLL_CLKSEL, S3C_CLK_SRC);
	/* AUDIO0 sel : FOUTepll */
	writel((readl(S3C_CLK_SRC)&~(0x7<<7))|(0<<7), S3C_CLK_SRC);

	/* CLK_DIV2 setting */
	writel(0x0,S3C_CLK_DIV2);

	switch (params_rate(params)) {
	case 8000:
		pll_out = 12288000;
		prescaler = 8;
		break;
	case 11025:
		pll_out = 11289600;
		prescaler = 8; 
		break;
	case 16000:
		pll_out = 12288000;
		prescaler = 4; 
		break;
	case 22050:
		pll_out = 11289600;
		prescaler = 4; 
		break;
	case 32000:
		pll_out = 12288000;
		prescaler = 2; 
		break;
	case 44100:
		pll_out = 11289600;
		//prescaler = 6; 
		prescaler = 2; 
		break;
	case 48000:
		pll_out = 12288000;
		prescaler = 2; 
		break;
	case 88200:
		pll_out = 11289600;
		prescaler = 1; 
		break;
	case 96000:
		pll_out = 12288000;
		prescaler = 1;
		break;
	default:
		/* somtimes 32000 rate comes to 96000 
		   default values are same as 32000 */
		iismod |= S3C64XX_IISMOD_384FS;
		pll_out = 12288000;
		break;
	}

	/* set MCLK division for sample rate */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_S16_LE:
		iismod |= S3C64XX_IISMOD_256FS | S3C64XX_IISMOD_32FS;
		prescaler *= 3;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iismod |= S3C64XX_IISMOD_384FS | S3C64XX_IISMOD_48FS;
		prescaler *= 2;
		break;
	default:
		return -EINVAL;
	}

	prescaler = prescaler - 1; 
	writel(iismod , regs + S3C64XX_IISMOD);

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFM ); 
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS ); 
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8731_SYSCLK, pll_out,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set prescaler division for sample rate */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, S3C64XX_DIV_PRESCALER,
		(prescaler << 0x8));
	if (ret < 0)
		return ret;

	return 0;
}

#endif

/*
 * S5M8751 DAI operations.
 */
static struct snd_soc_ops smdk6410_ops = {
	.hw_params = smdk6410_hw_params,
};

static const struct snd_soc_dapm_widget wm8731_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_LINE("Line Jack", NULL),
	SND_SOC_DAPM_HP("Headset Jack", NULL),
};

/* example machine audio_mapnections */
static const struct snd_soc_dapm_route audio_map[] = {

	/* headset Jack  - in = micin, out = LHPOUT*/
	{"Headset Jack", NULL, "LHPOUT"},

	/* headphone connected to LHPOUT1, RHPOUT1 */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	/* speaker connected to LOUT, ROUT */
	{"Ext Spk", NULL, "ROUT"},
	{"Ext Spk", NULL, "LOUT"},

	/* mic is connected to MICIN (via right channel of headphone jack) */
	{"MICIN", NULL, "Mic Jack"},

	/* Same as the above but no mic bias for line signals */
	{"MICIN", NULL, "Line Jack"},
};

static int smdk6410_wm8731_init(struct snd_soc_codec *codec)
{
	int i;
	
	/* Add smdk6410 specific widgets */
	snd_soc_dapm_new_controls(codec, wm8731_dapm_widgets,ARRAY_SIZE(wm8731_dapm_widgets));

	/* set up smdk6410 specific audio paths */
	snd_soc_dapm_add_routes(codec, audio_map,ARRAY_SIZE(audio_map));

	/* No jack detect - mark all jacks as enabled */
	for (i = 0; i < ARRAY_SIZE(wm8731_dapm_widgets); i++)
		snd_soc_dapm_set_endpoint(codec,
					  wm8731_dapm_widgets[i].name, 1);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

static struct snd_soc_dai_link smdk6410_dai[] = {
{
	.name = "WM8731",
	.stream_name = "Playback",
	.cpu_dai = &s3c6410_i2s_v32_dai,
	.codec_dai = &wm8731_dai,
	.init = smdk6410_wm8731_init,
	.ops = &smdk6410_ops,
},
};

static struct snd_soc_machine smdk6410 = {
	.name = "smdk6410",
	.dai_link = smdk6410_dai,
	.num_links = ARRAY_SIZE(smdk6410_dai),
};

static struct wm8731_setup_data smdk6410_wm8731_setup = {
	.i2c_address = 0x1b,
};

static struct snd_soc_device smdk6410_snd_devdata = {
	.machine = &smdk6410,
	.platform = &s3c24xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8731,
	.codec_data = &smdk6410_wm8731_setup,
};

static struct platform_device *smdk6410_snd_device;

/* Added by woong */
static int audio_enable_state = 0;
//janged 이함수가 i2s clock 설정하는 것보다 먼저 호출 되어야 함 
void audio_enable_for_pm(void)
{
	u32 val;
	if (!audio_enable_state)
	{
		/* Configure the GPD pins in I2S and Pull-Up mode */
		val = __raw_readl(S3C64XX_GPDPUD);
		val &= ~((3<<0) | (3<<2) | (3<<4) | (3<<6) | (3<<8));
		val |= (2<<0) | (2<<2) | (2<<4) | (2<<6) | (2<<8);
		__raw_writel(val, S3C64XX_GPDPUD);

		val = __raw_readl(S3C64XX_GPDCON);
		val &= ~((0xf<<0) | (0xf<<4) | (0xf<<8) | (0xf<<12) | (0xf<<16));
		val |= (3<<0) | (3<<4) | (3<<8) | (3<<12) | (3<<16);
		__raw_writel(val, S3C64XX_GPDCON);

		audio_enable_state = 1;
		printk("kernel audio_enable_state = %d ++++++\n", audio_enable_state);
	}
}
EXPORT_SYMBOL(audio_enable_for_pm);

void audio_disable_for_pm(void)
{
	unsigned int gpio;
	unsigned int end;
	
	//GPD : I2S DISABLE : Low clock일때 약 8~10mA정도 떨어짐.
	end = S3C64XX_GPD(5);
	for (gpio = S3C64XX_GPD(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, 0);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	}
	audio_enable_state = 0;
}
EXPORT_SYMBOL(audio_disable_for_pm);
/* end */

static int __init smdk6410_audio_init(void)
{
	int ret;
	u32 val;

	/* Configure the GPD pins in I2S and Pull-Up mode */
	val = __raw_readl(S3C64XX_GPDPUD);
	val &= ~((3<<0) | (3<<2) | (3<<4) | (3<<6) | (3<<8));
	val |= (2<<0) | (2<<2) | (2<<4) | (2<<6) | (2<<8);
	__raw_writel(val, S3C64XX_GPDPUD);

	val = __raw_readl(S3C64XX_GPDCON);
	val &= ~((0xf<<0) | (0xf<<4) | (0xf<<8) | (0xf<<12) | (0xf<<16));
	val |= (3<<0) | (3<<4) | (3<<8) | (3<<12) | (3<<16);
	__raw_writel(val, S3C64XX_GPDCON);

	printk("\n\nGPDPUD=0x%8x : GPDCON=0x%8x\n\n", __raw_readl(S3C64XX_GPDPUD), __raw_readl(S3C64XX_GPDCON));
	smdk6410_snd_device = platform_device_alloc("soc-audio", 0);
	if (!smdk6410_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdk6410_snd_device, &smdk6410_snd_devdata);
	smdk6410_snd_devdata.dev = &smdk6410_snd_device->dev;
	ret = platform_device_add(smdk6410_snd_device);

	if (ret)
		platform_device_put(smdk6410_snd_device);
		
#ifndef CONFIG_SUPPORT_FACTORY
	/* Added by woong 
	   APP에서 음악을 Play하려고 할때만 I2S를 살려야 한다. 약 8mA정동 절약됨. */
	audio_disable_for_pm();
	/*end */
#endif
	return ret;
}

static void __exit smdk6410_audio_exit(void)
{
	platform_device_unregister(smdk6410_snd_device);
}

module_init(smdk6410_audio_init);
module_exit(smdk6410_audio_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC SMDK6410 WM8751");
MODULE_LICENSE("GPL");
