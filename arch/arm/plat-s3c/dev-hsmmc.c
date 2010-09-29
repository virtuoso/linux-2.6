/* linux/arch/arm/plat-s3c/dev-hsmmc.c
 *
 * Copyright (c) 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C series device definition for hsmmc devices
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

static struct resource s3c_hsmmc_resource[] = {
	[0] = {
		.start = S3C_PA_HSMMC0,
		.end   = S3C_PA_HSMMC0 + S3C_SZ_HSMMC - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_HSMMC0,
		.end   = IRQ_HSMMC0,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_hsmmc_dmamask = 0xffffffffUL;

//janged add start
#if 0
static void setup_sdhci0_irq_cd (void)
{
	/* init GPIO as a ext irq */
	s3c_gpio_cfgpin(S3C64XX_GPN(13), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(S3C64XX_GPN(13), S3C_GPIO_PULL_NONE);

	set_irq_type(S3C_EINT(13), IRQ_TYPE_EDGE_BOTH);
}


static uint detect_sdhci0_irq_cd (void)
{
	#if 0
	detect = readl(S3C64XX_GPNDAT);
	detect &= 0x2000;	/* GPN13 */


	return (!detect);
	#else
	//janged
	//MOVI NAND�� �׻� ON
	return 1;
	#endif
}
#endif
//janged add end
struct s3c_sdhci_platdata s3c_hsmmc0_def_platdata = {
	.max_width	= 4,
	//.host_caps	= (MMC_CAP_4_BIT_DATA | MMC_CAP_MMC_HIGHSPEED |MMC_CAP_SD_HIGHSPEED | MMC_CAP_ON_BOARD),
	.host_caps	= (MMC_CAP_4_BIT_DATA | MMC_CAP_ON_BOARD | MMC_CAP_BOOT_ONTHEFLY),

	//janged
//	.cfg_ext_cd	= setup_sdhci0_irq_cd,
//	.detect_ext_cd	= detect_sdhci0_irq_cd,
//	.ext_cd		= S3C_EINT(13),
};

struct platform_device s3c_device_hsmmc0 = {
	.name		= "s3c-sdhci",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc_resource),
	.resource	= s3c_hsmmc_resource,
	.dev		= {
		.dma_mask		= &s3c_device_hsmmc_dmamask,
		.coherent_dma_mask	= 0xffffffffUL,
		.platform_data		= &s3c_hsmmc0_def_platdata,
	},
};

void s3c_sdhci0_set_platdata(struct s3c_sdhci_platdata *pd)
{
	struct s3c_sdhci_platdata *set = &s3c_hsmmc0_def_platdata;

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

/* Added by woong for sleep mode */
void setup_sdhci0_irq_cd_sleep (void)
{
	/* init GPIO as a ext irq */
	s3c_gpio_cfgpin(S3C64XX_GPN(13), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(S3C64XX_GPN(13), S3C_GPIO_PULL_NONE);

	set_irq_type(S3C_EINT(13), IRQ_TYPE_EDGE_BOTH);
}
EXPORT_SYMBOL(setup_sdhci0_irq_cd_sleep);

