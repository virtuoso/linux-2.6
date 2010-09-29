/* linux/arch/arm/plat-s3c/dev-hsmmc1.c
 *
 * Copyright (c) 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C series device definition for hsmmc device 1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>

#include <mach/map.h>
#include <plat/sdhci.h>
#include <plat/devs.h>
#include <plat/cpu.h>

//janged add start
#include <linux/io.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-gpio.h>
//janged add end

static struct resource s3c_hsmmc1_resource[] = {
	[0] = {
		.start = S3C_PA_HSMMC1,
		.end   = S3C_PA_HSMMC1 + S3C_SZ_HSMMC - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_HSMMC1,
		.end   = IRQ_HSMMC1,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_hsmmc1_dmamask = 0xffffffffUL;

//janged add start
static void setup_sdhci1_irq_cd (void)
{
	/* init GPIO as a ext irq */
	s3c_gpio_cfgpin(S3C64XX_GPG(6), S3C_GPIO_SFN(3));
	s3c_gpio_setpull(S3C64XX_GPG(6), S3C_GPIO_PULL_DOWN);

	
	set_irq_type(S3C_EINT(6), IRQ_TYPE_EDGE_BOTH);
}

static uint detect_sdhci1_irq_cd (void)
{
	uint detect;

	detect = readl(S3C64XX_GPGDAT);
	detect &= 0x0040;	/* GPG6 */

	return (!detect);
}
//janged add end

struct s3c_sdhci_platdata s3c_hsmmc1_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA | MMC_CAP_MMC_HIGHSPEED |
				MMC_CAP_SD_HIGHSPEED),
	//janged
	.cfg_ext_cd	= setup_sdhci1_irq_cd,
	.detect_ext_cd	= detect_sdhci1_irq_cd,
	.ext_cd		= S3C_EINT(6),
};

struct platform_device s3c_device_hsmmc1 = {
	.name		= "s3c-sdhci",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc1_resource),
	.resource	= s3c_hsmmc1_resource,
	.dev		= {
		.dma_mask		= &s3c_device_hsmmc1_dmamask,
		.coherent_dma_mask	= 0xffffffffUL,
		.platform_data		= &s3c_hsmmc1_def_platdata,
	},
};

void s3c_sdhci1_set_platdata(struct s3c_sdhci_platdata *pd)
{
	struct s3c_sdhci_platdata *set = &s3c_hsmmc1_def_platdata;

	set->max_width = pd->max_width;

	if (pd->host_caps)
		set->host_caps = pd->host_caps;
	if (pd->cfg_gpio)
		set->cfg_gpio = pd->cfg_gpio;
	if (pd->cfg_card)
		set->cfg_card = pd->cfg_card;
	if (pd->cfg_ext_cd)
		set->cfg_ext_cd = pd->cfg_ext_cd;
	if (pd->detect_ext_cd)
		set->detect_ext_cd = pd->detect_ext_cd;
	if (pd->ext_cd)
		set->ext_cd = pd->ext_cd;
}

