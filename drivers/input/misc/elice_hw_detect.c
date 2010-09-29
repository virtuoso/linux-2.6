/*
 * drivers/input/misc/elice_hw_detect.c
 *
 * Copyright (C) 1999-2009 iriver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Detect hw detect pin for elice
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

#include <linux/irq.h>
#include <linux/interrupt.h>


#include <asm/io.h>
#include <linux/clk.h>
//#include <asm/plat-s3c/regs-adc.h>
#include <linux/gpio.h>
#include <asm/io.h>

#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>



//#define CONFIG_HW_DETECT_DEBUG//debuger enable.
#ifdef CONFIG_HW_DETECT_DEBUG
#define DPRINT(X...) do { printk("%s(): ", __func__); printk(X); } while(0)
#else
#define DPRINT(X...) do { } while(0)
#endif//CONFIG_HW_DETECT_DEBUG



//
//timer define.
//
#define CONFIG_HW_DETECT_TIMER //timeer enable.

#ifdef CONFIG_HW_DETECT_TIMER
#define KEY_DEBOUNCE_TIME	(HZ/5)	/* 200ms */

#ifndef KEY_RELEASED
#define KEY_RELEASED	0
#endif

#ifndef KEY_PRESSED
#define KEY_PRESSED	1
#endif

#define TYPE_DETECT_NULL		0
#define TYPE_DETECT_HOLD_KEY	100
#define TYPE_DETECT_EARJACK		101
#endif//CONFIG_HW_DETECT_TIMER


//
// Pin Description
//

// for earjack.
#define GPIO_HWDETECT_ELICE_EARJACK			S3C64XX_GPK(7)//S3C_GPK7
#define GPIO_HWDETECT_ELICE_EARJACK_MD		S3C_GPIO_INPUT//S3C_GPK7_INP
#define IS_EARJACK_DETECT		(!gpio_get_value(GPIO_HWDETECT_ELICE_EARJACK))//(low)

// for hold key.
#define GPIO_HWKEY_ELICE_HOLD			S3C64XX_GPN(11)//S3C_GPN11
#define GPIO_HWKEY_ELICE_HOLD_MD		S3C_GPIO_INPUT//S3C_GPN11_INP
#define IS_HOLD_KEY_DETECT		(!gpio_get_value(GPIO_HWKEY_ELICE_HOLD))

// for hold key.
#define GPIO_DETECT_ELICE_USB			S3C64XX_GPM(3)//S3C_GPM3
#define GPIO_DETECT_ELICE_USB_MD		S3C_GPIO_INPUT//S3C_GPN3_INP
#define IS_USB_DETECT			(gpio_get_value(GPIO_DETECT_ELICE_USB))

//
// global variable.
//
#ifdef CONFIG_HW_DETECT_TIMER
static struct timer_list hw_detect_timer;

static unsigned int last_state_detect_earjack = KEY_RELEASED;
#endif//CONFIG_HW_DETECT_TIMER

extern void clear_sleep_count(void);
unsigned char curHoldKeyState = 0;
void set_hold_key_state(unsigned char state)
{
	curHoldKeyState = state;
}

unsigned char get_hold_key_state(void)
{
	return curHoldKeyState;
}
EXPORT_SYMBOL(get_hold_key_state);

//
// for hw detect working...
//
#ifdef CONFIG_HW_DETECT_TIMER
extern void interrupt_externel_event(void);
static void hw_detect_worker(void)
{
	unsigned int nType = TYPE_DETECT_NULL;
	unsigned int curr_state_detect_earjack = (IS_EARJACK_DETECT) ? KEY_PRESSED : KEY_RELEASED;
	unsigned int curr_state_detect_holdkey = (IS_HOLD_KEY_DETECT) ? KEY_PRESSED : KEY_RELEASED;
	/* unsigned int curr_state_detect_usb = (IS_USB_DETECT) ? KEY_PRESSED : KEY_RELEASED; */
	static unsigned int prev_hold_key_state = 10;
	/* static unsigned int prev_usb_state = 10; */

	//
	// check current key type.
	//
	if(curr_state_detect_earjack == KEY_PRESSED)
	{
		nType = TYPE_DETECT_EARJACK;
	}

	if(curr_state_detect_holdkey == KEY_PRESSED)
	{
		set_hold_key_state(1);
	}
	else
	{
		set_hold_key_state(0);
	}
/*
	if (prev_usb_state != curr_state_detect_usb)
	{	
		interrupt_externel_event();
		clear_sleep_count();
		printk("############## USB STATE CHANGED #############\n");
	}
*/	
	if (prev_hold_key_state != curr_state_detect_holdkey)
	{
		interrupt_externel_event();
		clear_sleep_count();
	}

	prev_hold_key_state = curr_state_detect_holdkey;
	/* prev_usb_state = curr_state_detect_usb; */

	// 
	// key release
	//
	if (last_state_detect_earjack == KEY_PRESSED && curr_state_detect_earjack != KEY_PRESSED) 
	{			
		DPRINT("earjack disconnected!\n");
		last_state_detect_earjack = KEY_RELEASED;
		//speaker관련 janged
//		s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);//s3c_gpio_cfgpin(S3C_GPK3, S3C_GPK3_OUTP);
//		gpio_set_value(S3C64XX_GPK(3), 1);//s3c_gpio_setpin(S3C_GPK3, 1);
	}




	//
	// send current key.
	//
	switch (nType)
	{
		case TYPE_DETECT_EARJACK:
		{
			if (last_state_detect_earjack!= curr_state_detect_earjack&& curr_state_detect_earjack== KEY_PRESSED) 
			{
				DPRINT("earjack connected!\n");
				last_state_detect_earjack= KEY_PRESSED;

				//speaker관련 janged
//				s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);//s3c_gpio_cfgpin(S3C_GPK3, S3C_GPK3_OUTP);
//				gpio_set_value(S3C64XX_GPK(3), 0);//s3c_gpio_setpin(S3C_GPK3, 0);
			}
			else if(curr_state_detect_earjack== KEY_PRESSED)
			{
				//DPRINT("earjack long connected!\n");
				last_state_detect_earjack= KEY_PRESSED;
			}

			break;
		}
	}

}
#endif//CONFIG_HW_DETECT_TIMER


#ifdef CONFIG_HW_DETECT_TIMER
static void hw_detect_timer_handler(unsigned long data)
{
	hw_detect_worker();

	mod_timer(&hw_detect_timer, jiffies + KEY_DEBOUNCE_TIME);
}
#endif//CONFIG_HW_DETECT_TIMER

static int __init elice_hw_detect_init(void)
{
	//int ret;
	
	DPRINT("hw detect config init!\n");

	//
	// gpio config and init.
	//

	// janged USB VBUS OFF
       s3c_gpio_cfgpin(S3C64XX_GPM(2), S3C_GPIO_OUTPUT);
       s3c_gpio_setpull(S3C64XX_GPM(2), S3C_GPIO_PULL_NONE);
       s3c_gpio_setpull(S3C64XX_GPM(3), S3C_GPIO_PULL_DOWN);
       gpio_set_value(S3C64XX_GPM(2), 0);

     

	// ear jack detect pin.
	s3c_gpio_cfgpin(GPIO_HWDETECT_ELICE_EARJACK, GPIO_HWDETECT_ELICE_EARJACK_MD);
	s3c_gpio_setpull(GPIO_HWDETECT_ELICE_EARJACK, S3C_GPIO_PULL_NONE);

	// hold key.(application에서 확인)
	s3c_gpio_cfgpin(GPIO_HWKEY_ELICE_HOLD, GPIO_HWKEY_ELICE_HOLD_MD);
	s3c_gpio_setpull(GPIO_HWKEY_ELICE_HOLD, S3C_GPIO_PULL_NONE);	

//	s3c_gpio_setpull(GPIO_HWKEY_ELICE_HOLD, S3C_GPIO_PULL_DOWN);	//janged test


#ifdef CONFIG_HW_DETECT_TIMER
	//
	// timer init.
	//
	init_timer(&hw_detect_timer);
	hw_detect_timer.function = hw_detect_timer_handler;

	hw_detect_timer_handler(0);
#endif//CONFIG_HW_DETECT_TIMER

	return 0;
}

static void __exit elice_hw_detect_exit(void)
{
	DPRINT("end hw detect!\n");

#ifdef CONFIG_HW_DETECT_TIMER
	del_timer_sync(&hw_detect_timer);
#endif//CONFIG_HW_DETECT_TIMER
}

/* Added by woong */
void delete_hw_detect_timer(void)
{
#ifdef CONFIG_HW_DETECT_TIMER
	printk("delete hw detect timer\n");
	del_timer_sync(&hw_detect_timer);
#endif//CONFIG_HW_DETECT_TIMER
}
EXPORT_SYMBOL(delete_hw_detect_timer);

module_init(elice_hw_detect_init);
module_exit(elice_hw_detect_exit);

MODULE_AUTHOR("Hyung-Seoung Yoo");
MODULE_DESCRIPTION ("Detect hw pins for elice.");
MODULE_LICENSE("GPL");
