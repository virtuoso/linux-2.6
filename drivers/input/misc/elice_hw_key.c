/*
 * drivers/input/misc/elice_hw_key.c
 *
 * Copyright (C) 1999-2009 iriver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Detect hw input key for elice
 *
 *
 * Author: Hyung-Seoung Yoo <billy.yoo@iriver.com>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <linux/gpio.h>
#include <asm/io.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/platform_device.h>

//#define CONFIG_HW_KEY_DEBUG	//janged 주석으로 막아야 powr off시 문제 없음 
#ifdef CONFIG_HW_KEY_DEBUG
#define DPRINT(X...) do { printk("[kernel] %s(): ", __func__); printk(X); } while(0)
#else
#define DPRINT(X...) do { } while(0)
#endif//CONFIG_HW_KEY_DEBUG

//woong
extern void key_pressed(void);

//
//Add define.
//
//#define KEY_DEBOUNCE_TIME	(HZ/5)	/* 200ms */

#ifndef KEY_RELEASED
#define KEY_RELEASED	0
#endif

#ifndef KEY_PRESSED
#define KEY_PRESSED	1
#endif

#define TYPE_KEY_NULL		0
#define TYPE_KEY_POWER		100



//
// Pin Description
//

#define GPIO_HWKEY_ELICE_POWER			S3C64XX_GPN(12)//S3C_GPN12
#define GPIO_HWKEY_ELICE_POWER_MD		S3C_GPIO_INPUT//S3C_GPN12_INP
#define IS_POWER_KEY_DET		(gpio_get_value(GPIO_HWKEY_ELICE_POWER))
#define IS_USB_DET				(gpio_get_value(S3C64XX_GPM(3)))


//
// global variable.
//
static struct timer_list hw_key_timer;

static unsigned int last_state_key_power = KEY_RELEASED;

static unsigned int force_power_off = 0;
static unsigned int force_power_off_cnt = 0;
static unsigned int power_key_cnt = 0;
static unsigned short KEY_DEBOUNCE_TIME = 40;

/* Added by woong */
static unsigned int wasInSuspendState = 0;
/* edn */
//
// for report key value.
//

#define ENABLE_DEV_HW_KEY////dev file..

#ifdef ENABLE_DEV_HW_KEY
#include "linux/fs.h"
#include "linux/fcntl.h"
#include "elice_hw_key_config.h"
#include "asm/uaccess.h"

unsigned char send_key_type_0 = KEY_TYPE_NULL;
unsigned char send_key_type_1 = KEY_TYPE_NULL;
unsigned char send_key_type_2 = KEY_TYPE_NULL;
unsigned char send_key_type_3 = KEY_TYPE_NULL;

void set_report_key_value(unsigned char type_0, unsigned char type_1, unsigned char type_2, unsigned char type_3)
{
	send_key_type_0 = type_0;
	send_key_type_1 = type_1;
	send_key_type_2 = type_2;
	send_key_type_3 = type_3;
}
#endif//ENABLE_DEV_HW_KEY

//
// for hw key working...
//


extern void ebook_external_input_report_key(unsigned int code, int value);
extern void ebook_external_poweroff_display(void);
extern void ebook_external_ready_poweroff(void);
extern unsigned char get_poweroff_state_in_sleep(void);
extern void set_poweroff_state_in_sleep(unsigned char val);

//janged
static void elice_hw_key_exit_force(void);

void set_poweroff_key_chk_loop_time(unsigned short value)
{
	KEY_DEBOUNCE_TIME = value;
}
EXPORT_SYMBOL(set_poweroff_key_chk_loop_time);

static void hw_key_worker(void)
{
	unsigned int nType = TYPE_KEY_NULL;	
	unsigned int curr_state_power_key = (IS_POWER_KEY_DET) ? KEY_PRESSED : KEY_RELEASED;

	/* woong : sleep mode에서 wakeup할때 들어오는 키는 무시해야 한다*/
	if (wasInSuspendState)
	{
		DPRINT("was in sleep mode so we have to drop all key interrupt\n");
		wasInSuspendState = 0;
		return;
	}

	//
	// check current key type.
	//
	if(curr_state_power_key == KEY_PRESSED)
	{
		nType = TYPE_KEY_POWER;
	}

	// 
	// key release
	//
	if (last_state_key_power == KEY_PRESSED && curr_state_power_key != KEY_PRESSED) 
	{			
		DPRINT("k-> power key released!\n");
		last_state_key_power = KEY_RELEASED;

		set_report_key_value(KEY_TYPE_NULL, KEY_TYPE_NULL, KEY_TYPE_NULL, KEY_TYPE_NULL);

		power_key_cnt = 0;
	}

	if (get_poweroff_state_in_sleep())
	{
		printk("k-> power key ready to poweroff !\n");
		nType = TYPE_KEY_POWER;
		last_state_key_power = curr_state_power_key = KEY_PRESSED;
		power_key_cnt = 8;
	}
	
	//
	// send current key.
	//
	switch (nType)
	{
		case TYPE_KEY_POWER:
		{
			key_pressed();
			if (last_state_key_power != curr_state_power_key && curr_state_power_key == KEY_PRESSED) 
			{
				DPRINT("k-> power key pressed!\n");
				last_state_key_power = KEY_PRESSED;

				set_report_key_value(KEY_TYPE_POWER, KEY_POWER_SHORT, KEY_TYPE_NULL, KEY_TYPE_NULL);

				force_power_off_cnt = 0;
				power_key_cnt = 0;//short key check.
			}
			else if(curr_state_power_key == KEY_PRESSED)
			{
				DPRINT("k-> power key long pressed!\n");
				last_state_key_power = KEY_PRESSED;

				if(power_key_cnt == 2)
				{
					//short power key
					set_report_key_value(KEY_TYPE_POWER, KEY_POWER_SHORT, KEY_TYPE_NULL, KEY_TYPE_NULL);
					ebook_external_input_report_key(7, 1);//pressed.
					ebook_external_input_report_key(7, 0);//release.
				}

				if(power_key_cnt == 8)
				{
					set_report_key_value(KEY_TYPE_POWER, KEY_POWER_LONG, KEY_TYPE_NULL, KEY_TYPE_NULL);
					//USB 충정 중 이거나 UMS상황에서 Power Key 안보내려고 
					if(IS_USB_DET)
					{
						power_key_cnt = 0;
						force_power_off = 1;//for high clock.						
					}
					else
					{
						//long power key
						ebook_external_input_report_key(22, 1);//pressed.
						ebook_external_input_report_key(22, 0);//release.
						power_key_cnt = 0;

						force_power_off = 1;//for high clock.

						set_poweroff_state_in_sleep(0);
						//janged
						//형승아 더이상 키 안 받으려고 이렇게 했다
						//그리고 여기서 power off 화면 그려주면 좋겠다.
						//Power key 누르고 power off 빨리 나왔으면 좋겠다고 하더라고 
						elice_hw_key_exit_force();
					}
				}
//woong
#if 0
				//janged 이 밑의 if문은 아무 소용 없는 코드 임
				//위의 elice_hw_key_exit_force 가 호출되면 driver 자체가 종료됨 
				//강제 power off는 app의 status_manager에서 하고 있음 
				//if(force_power_off_cnt>15)
				if(force_power_off_cnt>60)
				{
					if(force_power_off_cnt==0xff)
					{
						break;
					}

					do_changing_clock(6);
					//Added by woong
					del_timer_sync(&hw_key_timer);
					delete_hw_detect_timer();
					delete_all_dcc_timer();
					/*end woong */
					/* deleted by woong */
					//start_change_low_clock_sched();
					DPRINT("k-> power key ==> clock high!!\n");

					DPRINT("k-> force power off!!\n");
					ebook_external_poweroff_display();
					mdelay(50);
					ebook_external_ready_poweroff();
					
					//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_OUTP);
					s3c_gpio_cfgpin(S3C64XX_GPK(6), S3C_GPIO_INPUT);//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_INP);
					s3c_gpio_setpull(S3C64XX_GPK(6), S3C_GPIO_PULL_DOWN);

					DPRINT("end force power off!!\n");

					force_power_off_cnt = 0xff;

					force_power_off = 1;//for high clock.
					break;
				}
#endif
				power_key_cnt++;
				//force_power_off_cnt++;

				//janged
				if(IS_USB_DET)
				{
					force_power_off_cnt = 1;
				}
			}

			break;
		}
			
	}

}



static DECLARE_WORK(hw_key_worker_schedule, (void *)&hw_key_worker);
static void hw_key_timer_handler(unsigned long data)
{
	//hw_key_worker();
	schedule_work(&hw_key_worker_schedule);

	mod_timer(&hw_key_timer, jiffies + KEY_DEBOUNCE_TIME);
}




#ifdef ENABLE_DEV_HW_KEY
#define ELICE_HW_KEY_DEV_NAME		"eliceHwKey"
#define ELICE_HW_KEY_DEV_MAJOR	238


static int elice_hw_key_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int elice_hw_key_release(struct inode *inode, struct file *file)
{
	return 0;
}


//max 4byte.
static ssize_t elice_hw_key_read(struct file *file, char __user *buf, size_t len,
                         loff_t *ppos)
{
	int count=4;

	put_user(send_key_type_0, (char *) &buf[0]);
	put_user(send_key_type_1, (char *) &buf[1]);
	put_user(send_key_type_2, (char *) &buf[2]);
	put_user(send_key_type_3, (char *) &buf[3]);

	return count;
}


static const struct file_operations elice_hw_key_fops = {
	.owner		= THIS_MODULE,
	.read		= elice_hw_key_read,
	.open		= elice_hw_key_open,
	.release		= elice_hw_key_release,
};

#endif//ENABLE_DEV_HW_KEY

/* Added by woong for sleep mode */
#ifdef CONFIG_PM
static int __init elice_powerkey_probe(struct platform_device *pdev)
{
	printk("elice_powerkey_probe !!!!!!!!!!!\n");
	return 0;
}
static int elice_powerkey_remove(struct platform_device *pdev)
{
	printk("elice_powerkey_remove !!!!!!!!\n");
	return 0;
}
static int elice_powerkey_suspend(struct platform_device *dev, pm_message_t state)
{
	printk("elice_powerkey_suspend\n");
	wasInSuspendState = 1;
	return 0;
}

static int elice_powerkey_resume(struct platform_device *dev)
{
	//woong : Sleep Mode 이후 GPIO초기화 해줘야쥐..
	printk("elice_powerkey_resume\n");

	return 0;
}
#else
#define elice_powerkey_suspend NULL
#define elice_powerkey_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver elice_powerkey_driver = {
	.probe		= elice_powerkey_probe,
	.remove		= elice_powerkey_remove,
	.suspend	= elice_powerkey_suspend,
	.resume		= elice_powerkey_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "elice-powerkey",
	},
};
//end woong

static int __init elice_hw_key_init(void)
{
	DPRINT("hw key config init!\n");
	//
	// gpio config and init.
	//

	// power key.
	s3c_gpio_cfgpin(GPIO_HWKEY_ELICE_POWER, GPIO_HWKEY_ELICE_POWER_MD);
	s3c_gpio_setpull(GPIO_HWKEY_ELICE_POWER, S3C_GPIO_PULL_NONE);

	//
	// timer init.
	//
	init_timer(&hw_key_timer);
	hw_key_timer.function = hw_key_timer_handler;
	hw_key_timer_handler(0);


	#ifdef ENABLE_DEV_HW_KEY
	if (register_chrdev(ELICE_HW_KEY_DEV_MAJOR, ELICE_HW_KEY_DEV_NAME, &elice_hw_key_fops) < 0) 
	{
		printk("fail!! register_chrdev()\n");	
	}
	#endif//ENABLE_DEV_HW_KEY	

	//woong
	if (!platform_driver_register(&elice_powerkey_driver) )
	   printk("elice powerkey register success !!!\n");
	else
	   printk("elice powerkey register error !!!\n");

	return 0;
}

static void __exit elice_hw_key_exit(void)
{
	DPRINT("end key!\n");
	del_timer_sync(&hw_key_timer);

	#ifdef ENABLE_DEV_HW_KEY
	unregister_chrdev(ELICE_HW_KEY_DEV_MAJOR, ELICE_HW_KEY_DEV_NAME);
	#endif//ENABLE_DEV_HW_KEY

	//woong
	platform_driver_unregister(&elice_powerkey_driver);
}

//janged
static void elice_hw_key_exit_force(void)
{

	return;		//절대 없으면 안됨 반드시 return해야함 부팅시 롱키 잡으면 power key 그 이후 안 먹음 

	DPRINT("end key!\n");
	del_timer_sync(&hw_key_timer);

	#ifdef ENABLE_DEV_HW_KEY
	unregister_chrdev(ELICE_HW_KEY_DEV_MAJOR, ELICE_HW_KEY_DEV_NAME);
	#endif//ENABLE_DEV_HW_KEY

	//woong
	platform_driver_unregister(&elice_powerkey_driver);
}

module_init(elice_hw_key_init);
module_exit(elice_hw_key_exit);

MODULE_AUTHOR("Hyung-Seoung Yoo");
MODULE_DESCRIPTION ("Detect manul key for elice.");
MODULE_LICENSE("GPL");
