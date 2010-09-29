/* linux/arch/arm/mach-s3c6400/include/mach/system.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S3C6400 - system implementation
 */

#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H __FILE__

//#include <linux/io.h>
//#include <mach/map.h>
//#include <plat/regs-watchdog.h>


#include <mach/hardware.h>
#include <asm/io.h>

#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>
#include <plat/regs-watchdog.h>
#include <mach/map.h>


void (*s3c64xx_idle)(void);
void (*s3c64xx_reset_hook)(void);

void s3c64xx_default_idle(void)
{
	/* nothing here yet */
}
	
static void arch_idle(void)
{
	if (s3c64xx_idle != NULL)
		(s3c64xx_idle)();
	else
		s3c64xx_default_idle();
}

static void arch_reset(char mode)
{
	//janged
	#if 0
	void __iomem	*wdt_reg;
	wdt_reg = ioremap(S3C64XX_PA_WATCHDOG,S3C64XX_SZ_WATCHDOG);

	/* nothing here yet */

	writel(S3C2410_WTCNT_CNT, wdt_reg + S3C2410_WTCNT_OFFSET);	/* Watchdog Count Register*/
	writel(S3C2410_WTCNT_CON, wdt_reg + S3C2410_WTCON_OFFSET);	/* Watchdog Controller Register*/
	writel(S3C2410_WTCNT_DAT, wdt_reg + S3C2410_WTDAT_OFFSET);	/* Watchdog Data Register*/
	#else
       /* nothing here yet */
        u32 reg;

        if (mode == 's') {
                cpu_reset(0);
        }

        reg = (u32) ioremap((unsigned long) S3C64XX_PA_SYSCON, SZ_4K);

	writel(0x6410, reg + S3C64XX_SW_RESET_OFF);	/* SW Register*/

	#endif
	
}

#endif /* __ASM_ARCH_IRQ_H */
