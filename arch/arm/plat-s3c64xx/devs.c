/* linux/arch/arm/plat-s3c64xx/devs.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * Base S3C64XX resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/devs.h>
#include <plat/adc.h>

//janged start
#include <linux/smc921x.h>
#include <mach/regs-mem.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/io.h>

#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
//janged end

//janged 
/* SMC9217 LAN via ROM interface */
static void elice_smsc_reset(void)
{
	#ifdef CONFIG_SMC921X
	unsigned int tmp;	
	#endif
//	gpio_direction_output(S3C64XX_GPN(11), 1);
	//s3c_gpio_pullup(S3C_GPL11, 0x2); /* pull-up */

	s3c_gpio_setpull(S3C64XX_GPL(11), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S3C64XX_GPL(11), S3C_GPIO_OUTPUT);
	gpio_set_value(S3C64XX_GPL(11), 0);

	#ifdef CONFIG_SMC921X	// eth off : 0 eth on : 1
	udelay(300); /* min 200us */
	gpio_set_value(S3C64XX_GPL(11), 1);

	tmp = __raw_readl(S3C64XX_SROM_BW);
	tmp &= ~(S3C64XX_SROM_BW_WAIT_ENABLE4_MASK | S3C64XX_SROM_BW_WAIT_ENABLE4_MASK | S3C64XX_SROM_BW_DATA_WIDTH4_MASK);
	tmp |= S3C64XX_SROM_BW_BYTE_ENABLE4_ENABLE | S3C64XX_SROM_BW_WAIT_ENABLE4_DISABLE| S3C64XX_SROM_BW_DATA_WIDTH4_16BIT;

	__raw_writel(tmp, S3C64XX_SROM_BW);

	__raw_writel(S3C64XX_SROM_BCn_TACS(0) | S3C64XX_SROM_BCn_TCOS(4) |S3C64XX_SROM_BCn_TACC(13) | 
				  S3C64XX_SROM_BCn_TCOH(1) | S3C64XX_SROM_BCn_TCAH(4) | S3C64XX_SROM_BCn_TACP(6) |
			S3C64XX_SROM_BCn_PMC_NORMAL, S3C64XX_SROM_BC4);

	s3c_gpio_setpull(S3C64XX_GPN(4), S3C_GPIO_PULL_NONE);
	set_irq_type(IRQ_EINT(4), IRQ_TYPE_LEVEL_HIGH);
	#endif
}

static struct smsc921x_platform_config elice_sm921x_config = {/* chip internal */
	.irq_polarity   = 1, /* 1: high-active, 0: low-active */
	.irq_type       = 1, /* 1: push-pull, 0: open-drain */
	.hw_reset	= elice_smsc_reset,
};
static struct resource s3c_smc911x_resources[] = {
      [0] = {
              .start  = S3C64XX_PA_SMC9115,
              .end    = S3C64XX_PA_SMC9115 + S3C64XX_SZ_SMC9115 - 1,
              .flags  = IORESOURCE_MEM,
      },
      [1] = {
              .start = IRQ_EINT(4),
              .end   = IRQ_EINT(4),
              .flags = IORESOURCE_IRQ,
        },
};

struct platform_device s3c_device_smc911x = {
      .name           = "smsc921x",
      .id             =  -1,
      .num_resources  = ARRAY_SIZE(s3c_smc911x_resources),
      .resource       = s3c_smc911x_resources,
      .dev	=
      {
      		.platform_data  = &elice_sm921x_config,
      },
};

/* NAND Controller */

static struct resource s3c_nand_resource[] = {
	[0] = {
		.start = S3C64XX_PA_NAND,
		.end   = S3C64XX_PA_NAND + S3C64XX_SZ_NAND - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device s3c_device_nand = {
	.name		  = "s3c-nand",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_nand_resource),
	.resource	  = s3c_nand_resource,
};

EXPORT_SYMBOL(s3c_device_nand);

/* OneNAND Controller */
static struct resource s3c_onenand_resource[] = {
	[0] = {
		.start = S3C64XX_PA_ONENAND,
		.end   = S3C64XX_PA_ONENAND + S3C_SZ_ONENAND - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device s3c_device_onenand = {
	.name		  = "onenand",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_onenand_resource),
	.resource	  = s3c_onenand_resource,
};

EXPORT_SYMBOL(s3c_device_onenand);

/* USB Host Controller */

static struct resource s3c_usb_resource[] = {
        [0] = {
                .start = S3C64XX_PA_USBHOST,
                .end   = S3C64XX_PA_USBHOST + S3C64XX_SZ_USBHOST - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_UHOST,
                .end   = IRQ_UHOST,
                .flags = IORESOURCE_IRQ,
        }
};

static u64 s3c_device_usb_dmamask = 0xffffffffUL;

struct platform_device s3c_device_usb = {
        .name             = "s3c2410-ohci",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_usb_resource),
        .resource         = s3c_usb_resource,
        .dev              = {
                .dma_mask = &s3c_device_usb_dmamask,
                .coherent_dma_mask = 0xffffffffUL
        }
};

EXPORT_SYMBOL(s3c_device_usb);

/* USB Device (Gadget)*/

static struct resource s3c_usbgadget_resource[] = {
	[0] = {
		.start = S3C_PA_OTG,
		.end   = S3C_PA_OTG + S3C_SZ_OTG - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_OTG,
		.end   = IRQ_OTG,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_usbgadget = {
	.name		  = "s3c-usbgadget",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_usbgadget_resource),
	.resource	  = s3c_usbgadget_resource,
};

EXPORT_SYMBOL(s3c_device_usbgadget);

/* USB Device (OTG hcd)*/

static struct resource s3c_usb_otghcd_resource[] = {
	[0] = {
		.start = S3C_PA_OTG,
		.end   = S3C_PA_OTG + S3C_SZ_OTG - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_OTG,
		.end   = IRQ_OTG,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_usb_otghcd_dmamask = 0xffffffffUL;

struct platform_device s3c_device_usb_otghcd = {
	.name		= "s3c_otghcd",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb_otghcd_resource),
	.resource	= s3c_usb_otghcd_resource,
        .dev              = {
                .dma_mask = &s3c_device_usb_otghcd_dmamask,
                .coherent_dma_mask = 0xffffffffUL
        }
};

EXPORT_SYMBOL(s3c_device_usb_otghcd);

/* LCD Controller */

static struct resource s3c_lcd_resource[] = {
	[0] = {
		.start = S3C64XX_PA_LCD,
		.end   = S3C64XX_PA_LCD + SZ_1M - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCD_VSYNC,
		.end   = IRQ_LCD_SYSTEM,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_lcd_dmamask = 0xffffffffUL;

struct platform_device s3c_device_lcd = {
	.name		  = "s3c-lcd",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_lcd_resource),
	.resource	  = s3c_lcd_resource,
	.dev              = {
		.dma_mask		= &s3c_device_lcd_dmamask,
		.coherent_dma_mask	= 0xffffffffUL
	}
};

/* FIMG-2D controller */
static struct resource s3c_g2d_resource[] = {
	[0] = {
		.start	= S3C64XX_PA_G2D,
		.end	= S3C64XX_PA_G2D + S3C64XX_SZ_G2D - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_2D,
		.end	= IRQ_2D,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_g2d = {
	.name		= "s3c-g2d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_g2d_resource),
	.resource	= s3c_g2d_resource
};
EXPORT_SYMBOL(s3c_device_g2d);


/* FIMG-3D controller */
static struct resource s3c_g3d_resource[] = {
	[0] = {
		.start	= S3C64XX_PA_G3D,
		.end	= S3C64XX_PA_G3D + S3C64XX_SZ_G3D - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S3C6410_G3D,
		.end	= IRQ_S3C6410_G3D,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_g3d = {
	.name		= "s3c-g3d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_g3d_resource),
	.resource	= s3c_g3d_resource
};
EXPORT_SYMBOL(s3c_device_g3d);


/* VPP controller */
static struct resource s3c_vpp_resource[] = {                                                                         
        [0] = { 
            .start = S3C6400_PA_VPP,
            .end   = S3C6400_PA_VPP + S3C_SZ_VPP - 1, 
            .flags = IORESOURCE_MEM,
        },
        [1] = { 
               .start = IRQ_POST0, 
               .end   = IRQ_POST0,
	      .flags = IORESOURCE_IRQ,             
	 } 
};                                                                                                                    

struct platform_device s3c_device_vpp = {  
        .name             = "s3c-vpp",
	.id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_vpp_resource),	
	.resource         = s3c_vpp_resource,                                                                         
};
EXPORT_SYMBOL(s3c_device_vpp);

/* TV encoder */
static struct resource s3c_tvenc_resource[] = {
	[0] = {
		.start = S3C6400_PA_TVENC,
		.end   = S3C6400_PA_TVENC + S3C_SZ_TVENC - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TVENC,
		.end   = IRQ_TVENC,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_tvenc = {
	.name		  = "s3c-tvenc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_tvenc_resource),
	.resource	  = s3c_tvenc_resource,
};

EXPORT_SYMBOL(s3c_device_tvenc);

/* MFC controller */
static struct resource s3c_mfc_resource[] = {
	[0] = {
		.start	= S3C6400_PA_MFC,
		.end	= S3C6400_PA_MFC + S3C_SZ_MFC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MFC,
		.end	= IRQ_MFC,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_mfc = {
	.name		= "s3c-mfc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_mfc_resource),
	.resource	= s3c_mfc_resource
};

EXPORT_SYMBOL(s3c_device_mfc);

/* TV scaler */
static struct resource s3c_tvscaler_resource[] = {
	[0] = {
		.start = S3C6400_PA_TVSCALER,
		.end   = S3C6400_PA_TVSCALER + S3C_SZ_TVSCALER - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SCALER,
		.end   = IRQ_SCALER,
		.flags = IORESOURCE_IRQ,
	}

};

/* rotator interface */
static struct resource s3c_rotator_resource[] = {
        [0] = {
                .start = S3C6400_PA_ROTATOR,
                .end   = S3C6400_PA_ROTATOR + S3C_SZ_ROTATOR - 1,
                .flags = IORESOURCE_MEM,
                },
        [1] = {
                .start = IRQ_ROTATOR,
                .end   = IRQ_ROTATOR,
                .flags = IORESOURCE_IRQ,
        }
};

struct platform_device s3c_device_rotator = {
        .name             = "s3c-rotator",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_rotator_resource),
        .resource         = s3c_rotator_resource
};

EXPORT_SYMBOL(s3c_device_rotator);

/* JPEG controller  */
static struct resource s3c_jpeg_resource[] = {
        [0] = {
                .start = S3C6400_PA_JPEG,
                .end   = S3C6400_PA_JPEG + S3C_SZ_JPEG - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_JPEG,
                .end   = IRQ_JPEG,
                .flags = IORESOURCE_IRQ,
        }

};

struct platform_device s3c_device_jpeg = {
        .name             = "s3c-jpeg",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_jpeg_resource),
        .resource         = s3c_jpeg_resource,
};

struct platform_device s3c_device_tvscaler = {
	.name		  = "s3c-tvscaler",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_tvscaler_resource),
	.resource	  = s3c_tvscaler_resource,
};

EXPORT_SYMBOL(s3c_device_tvscaler);


/* ADC */
static struct resource s3c_adc_resource[] = {
	[0] = {
		.start = S3C_PA_ADC,
		.end   = S3C_PA_ADC + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_PENDN,
		.end   = IRQ_PENDN,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_ADC,
		.end   = IRQ_ADC,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device s3c_device_adc = {
	.name		  = "s3c-adc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_adc_resource),
	.resource	  = s3c_adc_resource,
};

void __init s3c_adc_set_platdata(struct s3c_adc_mach_info *pd)
{
	struct s3c_adc_mach_info *npd;

	npd = kmalloc(sizeof(*npd), GFP_KERNEL);
	if (npd) {
		memcpy(npd, pd, sizeof(*npd));
		s3c_device_adc.dev.platform_data = npd;
	} else {
		printk(KERN_ERR "no memory for ADC platform data\n");
	}
}
EXPORT_SYMBOL(s3c_device_adc);


static struct resource s3c_rtc_resource[] = {
	[0] = {
		.start = S3C_PA_RTC,
		.end   = S3C_PA_RTC + 0xff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_RTC_ALARM,
		.end   = IRQ_RTC_ALARM,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_RTC_TIC,
		.end   = IRQ_RTC_TIC,
		.flags = IORESOURCE_IRQ
	}
};

struct platform_device s3c_device_rtc = {
	.name		  = "s3c2410-rtc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_rtc_resource),
	.resource	  = s3c_rtc_resource,
};

EXPORT_SYMBOL(s3c_device_rtc);


/* Keypad interface */
static struct resource s3c_keypad_resource[] = {
	[0] = {
		.start = S3C_PA_KEYPAD,
		.end   = S3C_PA_KEYPAD+ S3C_SZ_KEYPAD - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KEYPAD,
		.end   = IRQ_KEYPAD,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_keypad = {
	.name		  = "s3c-keypad",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_keypad_resource),
	.resource	  = s3c_keypad_resource,
};
EXPORT_SYMBOL(s3c_device_keypad);

/* Added by woong : Power Key interface */
static struct resource elice_powerkey_resource[] = {
	[0] = {
		.start = S3C_PA_KEYPAD,
		.end   = S3C_PA_KEYPAD+ S3C_SZ_KEYPAD - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KEYPAD,
		.end   = IRQ_KEYPAD,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device elice_device_powerkey = {
	.name		  = "elice-powerkey",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(elice_powerkey_resource),
	.resource	  = elice_powerkey_resource,
};
EXPORT_SYMBOL(elice_device_powerkey);

/* SPI (0) */
static struct resource s3c_spi0_resource[] = {
	[0] = {
		.start = S3C_PA_SPI0,
		.end   = S3C_PA_SPI0 + S3C_SZ_SPI0 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI0,
		.end   = IRQ_SPI0,
		.flags = IORESOURCE_IRQ,
	}

};

static u64 s3c_device_spi0_dmamask = 0xffffffffUL;

struct platform_device s3c_device_spi0 = {
	.name		  = "sam-spi",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s3c_spi0_resource),
	.resource	  = s3c_spi0_resource,
        .dev              = {
                .dma_mask = &s3c_device_spi0_dmamask,
                .coherent_dma_mask = 0xffffffffUL
        }
};
EXPORT_SYMBOL(s3c_device_spi0);

/* SPI (1) */
static struct resource s3c_spi1_resource[] = {
	[0] = {
		.start = S3C_PA_SPI1,
		.end   = S3C_PA_SPI1 + S3C_SZ_SPI1 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI1,
		.end   = IRQ_SPI1,
		.flags = IORESOURCE_IRQ,
	}

};

static u64 s3c_device_spi1_dmamask = 0xffffffffUL;

struct platform_device s3c_device_spi1 = {
	.name		  = "sam-spi",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s3c_spi1_resource),
	.resource	  = s3c_spi1_resource,
        .dev              = {
                .dma_mask = &s3c_device_spi1_dmamask,
                .coherent_dma_mask = 0xffffffffUL
        }
};
EXPORT_SYMBOL(s3c_device_spi1);

/* Watchdog */
static struct resource s3c_wdt_resource[] = {
	[0] = {
		.start = S3C64XX_PA_WATCHDOG,
		.end   = S3C64XX_PA_WATCHDOG + S3C64XX_SZ_WATCHDOG - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_WDT,
		.end   = IRQ_WDT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_wdt = {
	.name             = "s3c2410-wdt",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(s3c_wdt_resource),
	.resource         = s3c_wdt_resource,
};

EXPORT_SYMBOL(s3c_device_wdt);

