/*
 * drivers/elice/elice_hw_board_version.c
 *
 * Copyright (C) 1999-2009 iriver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Detect hw board version for elice
 *
 *
 * Author: Hyung-Seoung Yoo <billy.yoo@iriver.com>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <plat/regs-adc.h>
#include <mach/map.h>
#include <linux/proc_fs.h>
#include "elice_hw_board_version.h"


#define AIN0		0
#define AIN1		1
#define AIN2		2
#define AIN3		3
#define AIN4		4
#define AIN5		5
#define AIN6		6
#define AIN7		7

extern int read_adc_ch(int ch);
extern int check_battery_level_for_bootup(int sequence);
extern unsigned int get_cur_battery_adc_value(void);
static char *hwrev;
static int elice_board_version;


#define REV_EVM1				"elice EVM 1" // 3.0v -> 932
#define MAX_REV_EVM1_ADC		1000
#define MIN_REV_EVM1_ADC		900

#define REV_WS1					"elice WS 1" // 2.8v -> 867
#define MAX_REV_WS1_ADC			900
#define MIN_REV_WS1_ADC			833

#define REV_ES1					"elice ES 1" // 2.6v -> 803
#define MAX_REV_ES1_ADC			833
#define MIN_REV_ES1_ADC			780

#if 1//
#define REV_TP1					"elice TP 1"// 2.4v -> 735
#define MAX_REV_TP1_ADC			780
#define MIN_REV_TP1_ADC			710

#define REV_MP1					"elice MP 1" // 2.2v -> 678
#define MAX_REV_MP1_ADC			710
#define MIN_REV_MP1_ADC			640

#define REV_MP2					"elice MP 2" // 2.0v -> 598
#define MAX_REV_MP2_ADC			640
#define MIN_REV_MP2_ADC			578

#define REV_MP3					"elice MP 3" // 1.8v -> 557
#define MAX_REV_MP3_ADC			578
#define MIN_REV_MP3_ADC			520

#define REV_MP2_EDU				"elice MP EDU" // 1.8v -> 557
#define MAX_REV_MP2_EDU_ADC		150
#define MIN_REV_MP2_EDU_ADC		120

#else
#define REV_ES2					"elice ES 2" // 2.4v -> 735
#define MAX_REV_ES2_ADC			780
#define MIN_REV_ES2_ADC			710

#define REV_TP1					"elice TP 1" // 2.2v -> 678
#define MAX_REV_TP1_ADC			710
#define MIN_REV_TP1_ADC			640

#define REV_MP1					"elice MP 1" // 2.0v -> 598
#define MAX_REV_MP1_ADC			640
#define MIN_REV_MP1_ADC			578

#define REV_MP2					"elice MP 2" // 1.8v -> 557
#define MAX_REV_MP2_ADC			578
#define MIN_REV_MP2_ADC			520

#define REV_MP2_EDU				"elice MP EDU" // 1.8v -> 557
#define MAX_REV_MP2_EDU_ADC		150
#define MIN_REV_MP2_EDU_ADC		120

#endif//0


static char * get_hw_rev(void)
{
	int adc, adc_total;
	char *rev=NULL;
	int try = 10;

	adc_total = 0;

	while(try-- > 0) 
	{
		adc = read_adc_ch(AIN7);
		adc_total += adc;
		//printk("rev adc = %d\n", adc);
	}

	adc = adc_total / 10;

	if (adc < MAX_REV_EVM1_ADC && adc > MIN_REV_EVM1_ADC) 
	{
		rev = REV_EVM1;
		elice_board_version = ELICE_BOARD_EVM1;
	} 
	else if (adc < MAX_REV_WS1_ADC && adc > MIN_REV_WS1_ADC) 
	{
		rev = REV_WS1;
		elice_board_version = ELICE_BOARD_WS1;
	} 
	else if (adc < MAX_REV_ES1_ADC && adc > MIN_REV_ES1_ADC) 
	{
		rev = REV_ES1;
		elice_board_version = ELICE_BOARD_ES1;
	}
#if 0//remove ES2
	else if (adc < MAX_REV_ES2_ADC && adc > MIN_REV_ES2_ADC) 
	{
		rev = REV_ES2;
		elice_board_version = ELICE_BOARD_ES2;
	}
#endif//0
	else if (adc < MAX_REV_TP1_ADC && adc > MIN_REV_TP1_ADC) 
	{
		rev = REV_TP1;
		elice_board_version = ELICE_BOARD_TP1;
	} 
	else if (adc < MAX_REV_MP1_ADC && adc > MIN_REV_MP1_ADC) 
	{
		rev = REV_MP1;
		elice_board_version = ELICE_BOARD_MP1;
	} 
	else if (adc < MAX_REV_MP2_ADC && adc > MIN_REV_MP2_ADC) 
	{
		rev = REV_MP2;
		elice_board_version = ELICE_BOARD_MP2;
	} 
	else if (adc < MIN_REV_MP2_EDU_ADC) 
	{
		rev = REV_MP2_EDU;
		elice_board_version = ELICE_BOARD_MP2_EDU;
	} 

	return rev;
}

int get_elice_board_version(void)
{
	return elice_board_version;
}
EXPORT_SYMBOL_GPL(get_elice_board_version);


extern void s1d13521if_extern_ready_Reset(void);
extern unsigned int get_elice_sw_dev_version(void);
extern unsigned int get_elice_sw_rel_version(void);


#define PROC_NAME_BOARD_EVM1		"ebook_evm1"
#define PROC_NAME_BOARD_WS1		"ebook_ws1"
#define PROC_NAME_BOARD_ES1		"ebook_es1"

#define PROC_VERSION		0
#define PROC_POWEROFF		1

static struct proc_dir_entry *board_version_dir;
static struct proc_dir_entry *version_file;
static struct proc_dir_entry *poweroff_file;

char *proc_entry_name;


#include <linux/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
extern void ebook_external_poweroff_display(void);
extern void ebook_external_ready_poweroff(void);
static int proc_board_version_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len =0;
	
	switch ((u32)data)
	{
		case PROC_VERSION:
		{
			unsigned int Dev_Ver	=	get_elice_sw_dev_version();
			unsigned int Rel_Ver	= 	get_elice_sw_rel_version();
	
			len = sprintf(page, "[dev ver.] %d.%d.%d.%d\n[rel ver.] %d.%d.%d.%d\n"
				, (Dev_Ver>>24 & 0xff), (Dev_Ver>>16 & 0xff), (Dev_Ver>>8 & 0xff), (Dev_Ver & 0xff)
				, (Rel_Ver>>24 & 0xff), (Rel_Ver>>16 & 0xff), (Rel_Ver>>8 & 0xff), (Rel_Ver & 0xff));
		}
	break;

		case PROC_POWEROFF:
		{
			printk("kernel power off!!\n");
			//speak off.
			s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);
			gpio_set_value(S3C64XX_GPK(3), 0);
			//mute.
			s3c_gpio_cfgpin(S3C64XX_GPK(4), S3C_GPIO_OUTPUT);
			gpio_set_value(S3C64XX_GPK(4), 1);

			ebook_external_poweroff_display();
			mdelay(50);
			ebook_external_ready_poweroff();
			
			//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_OUTP);
			s3c_gpio_cfgpin(S3C64XX_GPK(6), S3C_GPIO_INPUT);//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_INP);
			s3c_gpio_setpull(S3C64XX_GPK(6), S3C_GPIO_PULL_DOWN);

			printk("end kernel power off!!\n");
		}
		break;
	}

	return len;
}

int board_version_proc_init(void)
{	
	board_version_dir = proc_mkdir("board_version", NULL);

	if (!board_version_dir)
		return -ENOMEM;

	board_version_dir->owner = THIS_MODULE;

	if(elice_board_version == ELICE_BOARD_EVM1)
	{
		proc_entry_name = PROC_NAME_BOARD_EVM1;
	}
	else if(elice_board_version == ELICE_BOARD_WS1)
	{
		proc_entry_name = PROC_NAME_BOARD_WS1;
	}
	else// if(elice_board_version == ELICE_BOARD_ES1)
	{
		proc_entry_name = PROC_NAME_BOARD_ES1;
	}
	
	version_file = create_proc_read_entry(proc_entry_name, 0444, board_version_dir, proc_board_version_read, (void*)PROC_VERSION);

	if (version_file == NULL)
		return -ENOMEM;

	poweroff_file = create_proc_read_entry("poweroff", 0444, board_version_dir, proc_board_version_read, (void*)PROC_POWEROFF);

	if (poweroff_file == NULL)
		return -ENOMEM;

	return 1;
}

void board_version_proc_terminate(void)
{
	remove_proc_entry("poweroff", board_version_dir);
	remove_proc_entry(proc_entry_name, board_version_dir);
	remove_proc_entry("board_version",NULL);
}

#if 0
static void __iomem 	*chip_id_addr;
static unsigned long chipID = 0x0;
//#define CHIP_ID_ADDR	0x7e00e000
#define CHIP_ID_ADDR	0x7e00d000
unsigned long get_chip_id(void)
{
	return chipID;
}
#else
extern unsigned long elisamoviserial;
unsigned long get_chip_id(void)
{
	return elisamoviserial;
}
#endif
EXPORT_SYMBOL(get_chip_id);

#include <plat/regs-clock.h>
#include <plat/pll.h>
static int __init elice_hw_board_version_init(void)
{
	//int ret;
	unsigned int Dev_Ver	=	get_elice_sw_dev_version();
	unsigned int Rel_Ver	= 	get_elice_sw_rel_version();
	unsigned char wasInBatteryCharging = 0;

	printk("elice_hw_board_version_init()\n");

	//2009,11,16 : woong
#if 0
	__raw_writel(0xE11, S3C_APLL_LOCK);
	__raw_writel(0xE11, S3C_MPLL_LOCK);
#else
	__raw_writel(0x4b1, S3C_APLL_LOCK);
	__raw_writel(0x4b1, S3C_MPLL_LOCK);
#endif
	// janged 
	// chip id unique 하지 않음 
	#if 0
	chip_id_addr = ioremap(CHIP_ID_ADDR, 0x1000);
	if(chip_id_addr ==  NULL){
		printk("fail to ioremap() region\n");
	}
	chipID = readl(chip_id_addr + 0x00);
	printk("######## CHIP ID 0x%lx #########\n", chipID);
	iounmap(chip_id_addr);
	#endif
	//연결했을때랑 안했을때랑 약 12정도 차이남.
	//먼저 USB가 연결되었는지 확인하고.. 연결되어 있는데 0x2f9보다 높으면 부팅할 수 있는거삼..
	// 0x2f9보다 낮으면 계속충전...
	s3c_gpio_cfgpin(S3C64XX_GPM(3), S3C_GPIO_INPUT);
	while(1)
	{
		if(gpio_get_value(S3C64XX_GPM(3)))
		{
			wasInBatteryCharging = 1;
			if (get_cur_battery_adc_value() >= 0x2fc)
			{
				break;
			}
			//usb 연결.. 근데 한번만 읽어도 되겠지... 
			//printk("USB Cable detected.... !!");
		}
		else
		{
			//뺐을때는 어차피 다음 루틴에서 poweroff를 해야 할지 안할지를 결정하면 되니깐 무조건 break.
			if (wasInBatteryCharging)
				mdelay(500);
			break;
		}
		ssleep(2);
	}

	/* Added by woong */
	if (check_battery_level_for_bootup(0))
	{
		//이때는 정말로 battery가 부족하여 epd를 초기화도 못하는 상태 poweroff 시켜야 한다.
		printk("##### Low Battery !! so kernel power off!!\n");
		//speak off.
		s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);
		gpio_set_value(S3C64XX_GPK(3), 0);
		//mute.
		s3c_gpio_cfgpin(S3C64XX_GPK(4), S3C_GPIO_OUTPUT);
		gpio_set_value(S3C64XX_GPK(4), 1);

		//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_OUTP);
		s3c_gpio_cfgpin(S3C64XX_GPK(6), S3C_GPIO_INPUT);//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_INP);
		s3c_gpio_setpull(S3C64XX_GPK(6), S3C_GPIO_PULL_DOWN);
	}
	/* end */

	//가장먼저 epson칩 reset을 high로 올린다.
	s1d13521if_extern_ready_Reset();	
	
	//
	// get hw board version.
	//
	hwrev = get_hw_rev();

	printk(KERN_INFO "=========================\n");
	printk(KERN_INFO "[ebook Board] %s\n",hwrev);
	printk(KERN_INFO "[kernel ver.] 2.6.28\n");
	printk(KERN_INFO "[dev ver.] %d.%d.%d.%d\n", (Dev_Ver>>24 & 0xff), (Dev_Ver>>16 & 0xff), (Dev_Ver>>8 & 0xff), (Dev_Ver & 0xff));
	printk(KERN_INFO "[rel ver.] %d.%d.%d.%d\n", (Rel_Ver>>24 & 0xff), (Rel_Ver>>16 & 0xff), (Rel_Ver>>8 & 0xff), (Rel_Ver & 0xff));
	printk(KERN_INFO "=========================\n");

	board_version_proc_init();

	//led control(for factory waveform.)
	s3c_gpio_cfgpin(S3C64XX_GPL(13), S3C_GPIO_OUTPUT);
	gpio_set_value(S3C64XX_GPL(13), 1);
	msleep(10);
	gpio_set_value(S3C64XX_GPL(13), 0);
	
	return 0;
}

static void __exit elice_hw_board_version_exit(void)
{
	board_version_proc_terminate();
	
	printk("%s()\n", __func__);
}

module_init(elice_hw_board_version_init);
module_exit(elice_hw_board_version_exit);

MODULE_AUTHOR("Hyung-Seoung Yoo");
MODULE_DESCRIPTION ("Check elice board version.");
MODULE_LICENSE("GPL");
