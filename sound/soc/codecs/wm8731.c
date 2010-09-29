/*
 * wm8731.c  --  WM8731 ALSA SoC Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "wm8731.h"

//janged start
#include <linux/gpio.h>
#include <asm/io.h>

#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/clk.h>
//janged end

//janged
#include <linux/proc_fs.h>

#define WM8731_VERSION "0.13"
//#define WM8731_DEBUG

#ifdef WM8731_DEBUG
//#define dbg(format, arg...) //	printk(KERN_DEBUG AUDIO_NAME ": " format "\n" , ## arg)
#define dbg(x...) printk(x)
#else
#define dbg(format, arg...) do {} while (0)
#endif

/* Added by woong */
void wm8731_poweroff(void);
void wm8731_poweron(void);
/* end */
struct snd_soc_codec_device soc_codec_dev_wm8731;
static struct snd_soc_device *wm8731_socdev;
/* codec private data */
struct wm8731_priv {
	unsigned int sysclk;
};

/*
 * wm8731 register cache
 * We can't read the WM8731 register space when we are
 * using 2 wire for device control, so we cache them instead.
 * There is no point in caching the reset register
 */
static const u16 wm8731_reg[WM8731_CACHEREGNUM] = {
    0x0097, 0x0097, 0x0079, 0x0079,
    0x000a, 0x0008, 0x009f, 0x0002,
    0x0000, 0x0000
};

// ++++++++++++++ janged proc interface start
#ifdef CONFIG_PROC_FS
#define WM8731_PROC_NAME	"wm8731"
static struct proc_dir_entry *wm8731_dir = NULL;
static struct proc_dir_entry *registers_dir = NULL;

typedef struct wm8731_reg_entry {
	char *name;
	int  offset;
} wm8731_reg_entry_t;

static ssize_t wm8731_proc_read_reg(struct file * file, char * buf,
					size_t nbytes, loff_t *ppos) {
	//struct ac97_codec *codec = &s3c64xx_ac97_codec;
	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;

	struct inode * my_inode = file->f_dentry->d_inode;
	struct proc_dir_entry * dp;
	wm8731_reg_entry_t *current_reg = NULL;
	char outputbuf[15];
	int count;

	if (*ppos > 0)
		return 0;

	dp = PDE(my_inode);
	current_reg = (wm8731_reg_entry_t *)(dp->data);

	if (current_reg == NULL)
		return -EINVAL;

	count = sprintf(&outputbuf[0], "0x%04x\n",
		codec->read(codec, current_reg->offset));

	*ppos += count;

	if (count > nbytes)
		return -EINVAL;
	if (copy_to_user(buf, &outputbuf[0], count))
		return -EFAULT;

	return count;
}

static ssize_t wm8731_proc_write_reg(struct file * file, const char * buffer,
					size_t count, loff_t *ppos) {
//	struct ac97_codec *codec = &s3c64xx_ac97_codec;

	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	
	struct inode * my_inode = file->f_dentry->d_inode;
	struct proc_dir_entry * dp;
	wm8731_reg_entry_t *current_reg = NULL;
	unsigned long new_reg_value;
	char *endp;

//	dp = (struct proc_dir_entry *)my_inode->u.generic_ip;
	dp = PDE(my_inode);
	current_reg = (wm8731_reg_entry_t *)(dp->data);

	if (current_reg == NULL)
		return -EINVAL;

	new_reg_value = simple_strtoul(buffer, &endp, 0);
	codec->write(codec, current_reg->offset, new_reg_value & 0xFFFF);

	return (count + endp - buffer);
}

static struct file_operations wm8731_proc_reg_fops = {
	.read	= wm8731_proc_read_reg,
	.write	= wm8731_proc_write_reg,
};
static wm8731_reg_entry_t wm8731_registers[] = {
#include "wm8731_registers.h"
};


void __init wm8731_create_proc_interface(void)
{
	struct proc_dir_entry *entry;
	int i;

	wm8731_dir = proc_mkdir("driver/wm8731", NULL);
	registers_dir = proc_mkdir("registers", wm8731_dir);

	for (i = 0; i < ARRAY_SIZE(wm8731_registers); i++) 
	{
		entry = create_proc_entry(wm8731_registers[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  registers_dir);
		entry->proc_fops = &wm8731_proc_reg_fops;
		entry->data = &wm8731_registers[i];
	}
}
#endif


// ++++++++++++++ proc interface end


/*
 * read wm8731 register cache
 */
static inline unsigned int wm8731_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	dbg("Entered %s reg = %x value = %x\n", __FUNCTION__, reg, cache[reg]);
	
	if (reg == WM8731_RESET)
		return 0;
	if (reg >= WM8731_CACHEREGNUM)
		return -1;
	return cache[reg];
}

/*
 * write wm8731 register cache
 */
static inline void wm8731_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	dbg("Entered %s reg = %x value = %x\n", __FUNCTION__, reg, value);
	if (reg >= WM8731_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the WM8731 register space
 */
static int wm8731_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D9 WM8731 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

	dbg("Entered %s reg = %x, value = %x\n", __FUNCTION__, reg, value);
	wm8731_write_reg_cache (codec, reg, value);
	if (codec->hw_write(codec->control_data, data, 2) == 2)
	{
		return 0;
	}
	else
	{
		printk("%s Fail\n", __FUNCTION__);
		return -EIO;
	}
}

#define wm8731_reset(c)	wm8731_write(c, WM8731_RESET, 0)

static const char *wm8731_input_select[] = {"Line In", "Mic"};
static const char *wm8731_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};

static const struct soc_enum wm8731_enum[] = {
	SOC_ENUM_SINGLE(WM8731_APANA, 2, 2, wm8731_input_select),
	SOC_ENUM_SINGLE(WM8731_APDIGI, 1, 4, wm8731_deemph),
};

static const struct snd_kcontrol_new wm8731_snd_controls[] = {

SOC_DOUBLE_R("Master Playback Volume", WM8731_LOUT1V, WM8731_ROUT1V,
	0, 127, 0),
SOC_DOUBLE_R("Master Playback ZC Switch", WM8731_LOUT1V, WM8731_ROUT1V,
	7, 1, 0),

SOC_DOUBLE_R("Capture Volume", WM8731_LINVOL, WM8731_RINVOL, 0, 31, 0),
SOC_DOUBLE_R("Line Capture Switch", WM8731_LINVOL, WM8731_RINVOL, 7, 1, 1),

SOC_SINGLE("Mic Boost (+20dB)", WM8731_APANA, 0, 1, 0),
SOC_SINGLE("Capture Mic Switch", WM8731_APANA, 1, 1, 1),

SOC_SINGLE("Sidetone Playback Volume", WM8731_APANA, 6, 3, 1),

SOC_SINGLE("ADC High Pass Filter Switch", WM8731_APDIGI, 0, 1, 1),
SOC_SINGLE("Store DC Offset Switch", WM8731_APDIGI, 4, 1, 0),

SOC_ENUM("Playback De-emphasis", wm8731_enum[1]),

//janged add
SOC_SINGLE("Codec Power on", WM8731_PWR, 7, 0, 0),
};

/* add non dapm controls */
static int wm8731_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(wm8731_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
				  snd_soc_cnew(&wm8731_snd_controls[i],
						codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

/* Output Mixer */
static const struct snd_kcontrol_new wm8731_output_mixer_controls[] = {
SOC_DAPM_SINGLE("Line Bypass Switch", WM8731_APANA, 3, 1, 0),
SOC_DAPM_SINGLE("Mic Sidetone Switch", WM8731_APANA, 5, 1, 0),
SOC_DAPM_SINGLE("HiFi Playback Switch", WM8731_APANA, 4, 1, 0),
};

/* Input mux */
static const struct snd_kcontrol_new wm8731_input_mux_controls =
SOC_DAPM_ENUM("Input Select", wm8731_enum[0]);

static const struct snd_soc_dapm_widget wm8731_dapm_widgets[] = {
SND_SOC_DAPM_MIXER("Output Mixer", WM8731_PWR, 4, 1,
	&wm8731_output_mixer_controls[0],
	ARRAY_SIZE(wm8731_output_mixer_controls)),
SND_SOC_DAPM_DAC("DAC", "HiFi Playback", WM8731_PWR, 3, 1),
SND_SOC_DAPM_OUTPUT("LOUT"),
SND_SOC_DAPM_OUTPUT("LHPOUT"),
SND_SOC_DAPM_OUTPUT("ROUT"),
SND_SOC_DAPM_OUTPUT("RHPOUT"),
SND_SOC_DAPM_ADC("ADC", "HiFi Capture", WM8731_PWR, 2, 1),
SND_SOC_DAPM_MUX("Input Mux", SND_SOC_NOPM, 0, 0, &wm8731_input_mux_controls),
SND_SOC_DAPM_PGA("Line Input", WM8731_PWR, 0, 1, NULL, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias", WM8731_PWR, 1, 1),
SND_SOC_DAPM_INPUT("MICIN"),
SND_SOC_DAPM_INPUT("RLINEIN"),
SND_SOC_DAPM_INPUT("LLINEIN"),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* output mixer */
	{"Output Mixer", "Line Bypass Switch", "Line Input"},
	{"Output Mixer", "HiFi Playback Switch", "DAC"},
	{"Output Mixer", "Mic Sidetone Switch", "Mic Bias"},

	/* outputs */
	{"RHPOUT", NULL, "Output Mixer"},
	{"ROUT", NULL, "Output Mixer"},
	{"LHPOUT", NULL, "Output Mixer"},
	{"LOUT", NULL, "Output Mixer"},

	/* input mux */
	{"Input Mux", "Line In", "Line Input"},
	{"Input Mux", "Mic", "Mic Bias"},
	{"ADC", NULL, "Input Mux"},

	/* inputs */
	{"Line Input", NULL, "LLINEIN"},
	{"Line Input", NULL, "RLINEIN"},
	{"Mic Bias", NULL, "MICIN"},
};

static int wm8731_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8731_dapm_widgets,
				  ARRAY_SIZE(wm8731_dapm_widgets));

	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:4;
	u8 bosr:1;
	u8 usb:1;
};

/* codec mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	#if 1
	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0, 0x0},
	{18432000, 48000, 384, 0x0, 0x1, 0x0},
	{12000000, 48000, 250, 0x0, 0x0, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0x6, 0x0, 0x0},
	{18432000, 32000, 576, 0x6, 0x1, 0x0},
	{12000000, 32000, 375, 0x6, 0x0, 0x1},

	/* 8k */
	{12288000, 8000, 1536, 0x3, 0x0, 0x0},
	{18432000, 8000, 2304, 0x3, 0x1, 0x0},
	{11289600, 8000, 1408, 0xb, 0x0, 0x0},
	{16934400, 8000, 2112, 0xb, 0x1, 0x0},
	{12000000, 8000, 1500, 0x3, 0x0, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0x7, 0x0, 0x0},
	{18432000, 96000, 192, 0x7, 0x1, 0x0},
	{12000000, 96000, 125, 0x7, 0x0, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x8, 0x0, 0x0},
	{16934400, 44100, 384, 0x8, 0x1, 0x0},
	{12000000, 44100, 272, 0x8, 0x1, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0xf, 0x0, 0x0},
	{16934400, 88200, 192, 0xf, 0x1, 0x0},
	{12000000, 88200, 136, 0xf, 0x1, 0x1},

	/* 16k	janged add */
	#else
	/* 8k */
	{12288000, 8000, 1536, 0x3,   0x0, 0x0},
	{11289600, 8000, 1408, 0xb,   0x0, 0x0},
	{18432000, 8000, 2304, 0x3,   0x1, 0x0},
	{16934400, 8000, 2112, 0xb,   0x1, 0x0},
	{12000000, 8000, 1500, 0x3,   0x0, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0xC,  0x0, 0x0},
	{16934400, 11025, 1536, 0xC,  0x1, 0x0},
	{12000000, 11025, 1088, 0xC,  0x1, 0x1},

	/* 16k */
	{12288000, 16000, 768, 0x5, 0x0, 0x0},
	{18432000, 16000, 1152, 0x5, 0x1, 0x0},
	{12000000, 16000, 750, 0x5, 0x0, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0xD,  0x0, 0x0},
	{16934400, 22050, 768, 0xD,  0x1, 0x0},
	{12000000, 22050, 544, 0xD,  0x1, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0x6, 0x0, 0x0},
	{18432000, 32000, 576, 0x6, 0x1, 0x0},
	{12000000, 32000, 375, 0x6, 0x0, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x8, 0x0, 0x0},
	{16934400, 44100, 384, 0x8, 0x1, 0x0},
	{12000000, 44100, 272, 0x8, 0x1, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x0,  0x0, 0x0},
	{18432000, 48000, 384, 0x0,  0x1, 0x0},
	{12000000, 48000, 250, 0x0,  0x0, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0xf,  0x0, 0x0},
	{16934400, 88200, 192, 0xf,  0x1, 0x0},
	{12000000, 88200, 136, 0xf,  0x1, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0x7, 0x0, 0x0},
	{18432000, 96000, 192, 0x7, 0x1, 0x0},
	{12000000, 96000, 125, 0x7, 0x0, 0x1},
	#endif
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return 0;
}

static int poweroff = 0;
static int wm8731_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct wm8731_priv *wm8731 = codec->private_data;
	u16 iface;
	u16 srate;
	int i;

	iface = wm8731_read_reg_cache(codec, WM8731_IFACE) & 0xfff2;
	i = get_coeff(wm8731->sysclk, params_rate(params));
	srate = (coeff_div[i].sr << 2) |	(coeff_div[i].bosr << 1) | coeff_div[i].usb;
	//wm8731_write(codec, WM8731_SRATE, srate);
	wm8731_write(codec, WM8731_SRATE, 0x0000);
	dbg("%s %d, sysclk = %d, rate = %d, i=%d, srate = %d\n", __FUNCTION__, __LINE__, wm8731->sysclk, params_rate(params), i, srate);
	
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	}

	//wm8731_write(codec, WM8731_IFACE, iface);
	wm8731_write(codec, WM8731_IFACE, 0x0002);

	return 0;
}

static int wm8731_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;

	dbg("Entered %s \n", __FUNCTION__);

	/* set active */
	wm8731_write(codec, WM8731_ACTIVE, 0x0001);

	return 0;
}

static void wm8731_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	gpio_set_value(S3C64XX_GPK(4), 0);
	dbg("Entered %s\n", __FUNCTION__);
	/* deactivate */
	if (!codec->active) {
		udelay(50);
		wm8731_write(codec, WM8731_ACTIVE, 0x0);
	}
}

static int wm8731_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8731_read_reg_cache(codec, WM8731_APDIGI) & 0xfff7;

	if (mute)
	{
		wm8731_write(codec, WM8731_APDIGI, mute_reg | 0x8);
	}
	else
	{
		wm8731_write(codec, WM8731_APDIGI, mute_reg);
	}
	return 0;
}

static int wm8731_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct wm8731_priv *wm8731 = codec->private_data;

	//janged modify
	#if 0
	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		wm8731->sysclk = freq;
		return 0;
	}
	return -EINVAL;
	#else
		wm8731->sysclk = freq;
		return 0;
	#endif
}


static int wm8731_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	dbg("Entered %s \n", __FUNCTION__);
	#if 1
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface |= 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;

	default:
		break;
//		return -EINVAL;	janged break;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		iface |= 0x0002;
		break;
//		return -EINVAL;		janged
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
//		return -EINVAL;		janged
		break;
	}
	/* set iface */
	wm8731_write(codec, WM8731_IFACE, iface);
	#else
	wm8731_write(codec, WM8731_IFACE, 0x0002);
	#endif
	return 0;
}

/* Changed by woong */
static int wm8731_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 reg = 0; 
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* vref/mid, osc on, dac unmute */
		//wm8731_write(codec, WM8731_PWR, reg);
		break;
	case SND_SOC_BIAS_PREPARE:
//		dbg("F%s L%d level = %d, reg = %x\n", __FUNCTION__,__LINE__, level, reg);
		break;
	case SND_SOC_BIAS_STANDBY:
		/* everything off except vref/vmid, */
		reg = wm8731_read_reg_cache(codec, WM8731_PWR) & 0xff7f;
		wm8731_write(codec, WM8731_PWR, reg | 0x0040);
		dbg("F%s L%d level = %d, reg = %x\n", __FUNCTION__,__LINE__, level, reg);
		break;
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute, inactive */
		#if 1
		wm8731_write(codec, WM8731_ACTIVE, 0x0);
		wm8731_write(codec, WM8731_PWR, 0xffff);
		dbg("F%s L%d level = %d, reg = %x\n", __FUNCTION__,__LINE__, level, reg);
		#endif
		break;
	}
	codec->bias_level = level;
	return 0;
}

#define WM8731_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
		SNDRV_PCM_RATE_96000)

#define WM8731_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)

struct snd_soc_dai wm8731_dai = {
	.name = "WM8731",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,		//channels_min = 1		janged
		.channels_max = 2,
		.rates = WM8731_RATES,
		.formats = WM8731_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8731_RATES,
		.formats = WM8731_FORMATS,},
	.ops = {
		.prepare = wm8731_pcm_prepare,
		.hw_params = wm8731_hw_params,
		.shutdown = wm8731_shutdown,
	},
	.dai_ops = {
		.digital_mute = wm8731_mute,
		.set_sysclk = wm8731_set_dai_sysclk,
		.set_fmt = wm8731_set_dai_fmt,
	}
};
EXPORT_SYMBOL_GPL(wm8731_dai);

static int wm8731_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	printk("Entered %s \n", __FUNCTION__);

	wm8731_write(codec, WM8731_ACTIVE, 0x0);
	wm8731_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int wm8731_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* changed by woong */
	//return 0;

	printk("Entered %s \n", __FUNCTION__);
	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8731_reg); i++) {
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}
	wm8731_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	wm8731_set_bias_level(codec, codec->suspend_bias_level);
	return 0;
}
/*
 * initialise the WM8731 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int wm8731_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	int reg, ret = 0;

	//janged
	#ifdef CONFIG_PROC_FS
	wm8731_create_proc_interface();
	#endif

	dbg("Entered %s \n", __FUNCTION__);
	codec->name = "WM8731";
	codec->owner = THIS_MODULE;
	codec->read = wm8731_read_reg_cache;
	codec->write = wm8731_write;
	codec->set_bias_level = wm8731_set_bias_level;
	codec->dai = &wm8731_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ARRAY_SIZE(wm8731_reg);
	codec->reg_cache = kmemdup(wm8731_reg, sizeof(wm8731_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL)
		return -ENOMEM;

	//janged090330 Return ¹Þ¾Æ Ã³¸® ÇÏµµ·Ï 
	ret = wm8731_reset(codec);
	if(ret < 0)
	{
		printk(KERN_ERR "wm8731: failed to send reset\n");
		return -EIO;
	}

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "wm8731: failed to create pcms\n");
		goto pcm_err;
	}

	/* power on device */
	wm8731_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* set the update bits */
	reg = wm8731_read_reg_cache(codec, WM8731_LOUT1V);
	wm8731_write(codec, WM8731_LOUT1V, reg & ~0x0100);
	reg = wm8731_read_reg_cache(codec, WM8731_ROUT1V);
	wm8731_write(codec, WM8731_ROUT1V, reg & ~0x0100);
	reg = wm8731_read_reg_cache(codec, WM8731_LINVOL);
	wm8731_write(codec, WM8731_LINVOL, reg & ~0x0100);
	reg = wm8731_read_reg_cache(codec, WM8731_RINVOL);
	wm8731_write(codec, WM8731_RINVOL, reg & ~0x0100);

	wm8731_add_controls(codec);
	wm8731_add_widgets(codec);
	ret = snd_soc_register_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "wm8731: failed to register card\n");
		goto card_err;
	}

	/* power on device */
//	wm8731_set_bias_level(codec, SND_SOC_BIAS_OFF);
	wm8731_poweroff();
	
	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *wm8731_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

/*
 * WM8731 2 wire address is determined by GPIO5
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */

static int wm8731_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = wm8731_init(socdev);
	if (ret < 0)
		pr_err("failed to initialise WM8731\n");

	return ret;
}

static int wm8731_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id wm8731_i2c_id[] = {
	{ "wm8731", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8731_i2c_id);

static struct i2c_driver wm8731_i2c_driver = {
	.driver = {
		.name = "WM8731 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    wm8731_i2c_probe,
	.remove =   wm8731_i2c_remove,
	.id_table = wm8731_i2c_id,
};

static int wm8731_add_i2c_device(struct platform_device *pdev,
				 const struct wm8731_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&wm8731_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "wm8731", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",
			setup->i2c_bus);
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(&pdev->dev, "can't add i2c device at 0x%x\n",
			(unsigned int)info.addr);
		goto err_driver;
	}

	return 0;

err_driver:
	i2c_del_driver(&wm8731_i2c_driver);
	return -ENODEV;
}
#endif

#if defined(CONFIG_SPI_MASTER)
static int __devinit wm8731_spi_probe(struct spi_device *spi)
{
	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	codec->control_data = spi;

	ret = wm8731_init(socdev);
	if (ret < 0)
		dev_err(&spi->dev, "failed to initialise WM8731\n");

	return ret;
}

static int __devexit wm8731_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver wm8731_spi_driver = {
	.driver = {
		.name	= "wm8731",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= wm8731_spi_probe,
	.remove		= __devexit_p(wm8731_spi_remove),
};

static int wm8731_spi_write(struct spi_device *spi, const char *data, int len)
{
	struct spi_transfer t;
	struct spi_message m;
	u8 msg[2];

	if (len <= 0)
		return 0;

	msg[0] = data[0];
	msg[1] = data[1];

	spi_message_init(&m);
	memset(&t, 0, (sizeof t));

	t.tx_buf = &msg[0];
	t.len = len;

	spi_message_add_tail(&t, &m);
	spi_sync(spi, &m);

	return len;
}
#endif /* CONFIG_SPI_MASTER */

static int wm8731_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct wm8731_setup_data *setup;
	struct snd_soc_codec *codec;
	struct wm8731_priv *wm8731;
	int ret = 0;

	pr_info("WM8731 Audio Codec %s", WM8731_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	wm8731 = kzalloc(sizeof(struct wm8731_priv), GFP_KERNEL);
	if (wm8731 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = wm8731;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	wm8731_socdev = socdev;
	ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = wm8731_add_i2c_device(pdev, setup);
	}
#endif
#if defined(CONFIG_SPI_MASTER)
	if (setup->spi) {
		codec->hw_write = (hw_write_t)wm8731_spi_write;
		ret = spi_register_driver(&wm8731_spi_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add spi driver");
	}
#endif

	if (ret != 0) {
		kfree(codec->private_data);
		kfree(codec);
	}

	/* Added by woong */
	//save_reg = wm8731_read_reg_cache(codec, WM8731_PWR);
	//printk("\n##################### poewr on save_reg 0x%8x #####################3\n", save_reg);
	return ret;
}

/* power down chip */
static int wm8731_remove(struct platform_device *pdev)
{
	#if 1
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (codec->control_data)
		wm8731_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&wm8731_i2c_driver);
#endif
#if defined(CONFIG_SPI_MASTER)
	spi_unregister_driver(&wm8731_spi_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);
	#endif
	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8731 = {
	.probe = 	wm8731_probe,
	.remove = 	wm8731_remove,
	.suspend = 	wm8731_suspend,
	.resume =	wm8731_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8731);

/* Changed by woong */
extern void audio_disable_for_pm(void);

void wm8731_poweroff(void)
{
	#if 1
	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;

	if(!poweroff)
	{
		struct clk *clk;
		s3c_gpio_cfgpin(S3C64XX_GPK(4), S3C_GPIO_OUTPUT);
		gpio_set_value(S3C64XX_GPK(4), 0);
		mdelay(500);
		s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);//s3c_gpio_cfgpin(S3C_GPK3, S3C_GPK3_OUTP);
		gpio_set_value(S3C64XX_GPK(3), 0);//s3c_gpio_setpin(S3C_GPK3, 1);


		wm8731_write(codec, WM8731_ACTIVE, 0x0);
		wm8731_set_bias_level(codec, SND_SOC_BIAS_OFF);
		wm8731_set_bias_level(codec, codec->suspend_bias_level);
		poweroff = 1;
		printk("Codec Power off ++++++++\n");

		//janged add for clk
		clk = clk_get(NULL, "fout_epll");
		if(clk_get_rate(clk) != 67738000)
		{
			clk_disable(clk);
			clk_set_rate(clk, 67738000);
			clk_enable(clk);
		}

		audio_disable_for_pm();

	}
	#endif
}

void wm8731_poweron(void)
{
	#if 1
	struct snd_soc_device *socdev = wm8731_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	if(poweroff)
	{
		printk("Entered %s \n", __FUNCTION__);
		wm8731_reset(codec);
		wm8731_write(codec, WM8731_PWR, 0x4e);
		/* Sync reg_cache with the hardware */
		#if 1
		for (i = 0; i < ARRAY_SIZE(wm8731_reg); i++) {
			data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
			data[1] = cache[i] & 0x00ff;
			codec->hw_write(codec->control_data, data, 2);
		}
		#endif		
		wm8731_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		wm8731_set_bias_level(codec, codec->suspend_bias_level);

		poweroff = 0;
		printk("Codec Power on ++++++++\n");
	}
	#endif
}

EXPORT_SYMBOL_GPL(wm8731_poweron);
EXPORT_SYMBOL_GPL(wm8731_poweroff);

MODULE_DESCRIPTION("ASoC WM8731 driver");
MODULE_AUTHOR("Richard Purdie");
MODULE_LICENSE("GPL");
