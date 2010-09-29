/* Dynamic Clock Control
 *
 * Copyright (c) 2009 iRiver
 * Tom.Lee <tom.lee@iriver.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/console.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/map.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>

#include "elice_dcc.h"


#define DCC_DEBUG
#ifdef DCC_DEBUG
#define dccdbg(X...) do { printk("<kernel : dcc> %s(): ", __func__); printk(X); } while(0)
#else
#define dccdbg(format, arg...) do {} while (0)
#endif

#define DEV_NAME	"dcc"
#define DEV_MAJOR	240

#define MAX_APLL_RATIO	7	
#define MAX_HCLK2_RATIO	7	

#define ENABLE			1
#define DISABLE			0	

#ifdef CONFIG_SUPPORT_FACTORY
#define SUPPORT_SLEEP_MODE 0
#else
//#define CURRENT_OPTIMIZE 1
#define SUPPORT_SLEEP_MODE 1
#endif//SUPPORT_FACTORY

#define NEW_SLEEP_WAY 1// 1: 동작.

#define	FIN							12000000

#define LOOP_REWORK_PERIOD	(HZ*10) //10초마다 한번씩 체크하도록 하자..

#ifdef CURRENT_OPTIMIZE
#define REWORK_PERIOD	(310) //1.5초마다 한번씩 체크하도록 하자..
#else
#define REWORK_PERIOD	(365) //1.5초마다 한번씩 체크하도록 하자..
#endif

//#define LOW_HZ			(167)  
#ifdef CURRENT_OPTIMIZE
#define LOW_HZ			(68)  //low clock일때 118정도가 10초정도 이다.
#else
#define LOW_HZ			(135)  //low clock일때 118정도가 10초정도 이다.
#endif
#define SLEEP_WAIT_PRIOD	(LOW_HZ*1)	//Low Clock일때 10초이다.

//#define FORCE_SLEEP_TIME	12		//120초
//#define FORCE_SLEEP_TIME	2		//20초
//#define FORCE_SLEEP_TIME	1		//10초
#define FORCE_SLEEP_TIME	60		//10분
//#define FORCE_SLEEP_TIME	120		//20분

#define NUMBER_OF_STEP 	10
#define MDIV		0
#define PDIV		1
#define SDIV		2
#define ARM_RATIO 	3
#define HCLK_RATIO 	4
#define PCLK_RATIO 	5
#define CLKJPEG_RATIO 	6
#define CLKSECUR_RATIO 	7
#define MFC_RATIO 	8

#define CLOCK_MOD_HIGH    1
#define CLOCK_MOD_LOW     2

#define PIN_LOW 0
#define PIN_HIGH 1

//#define REQUEST_HARDWARE 1
#ifdef REQUEST_HARDWARE
extern void enable_all_clock(void);
#endif

//#define DO_NOT_ENTER_SLEEPMODE 1
//#define JANGED_MODIFY

enum
{
	SLEEP_NONE					= 0, //은동이만 아는 flag
	REQUEST_SLEEP_ENTER_1 		= 1,
	REQUEST_SLEEP_ENTER_2 		= 2,  //은동이는 몰라도됨.
	REQUEST_WAKEUP 				= 3,
	REQUEST_POWEROFF 			= 4,
	SLEEP_ON		 			= 5,
	USB_CONNECT					= 6,
	USB_DISCONNECT				= 7,
	USB_SELECTED_CONNECTION_WAY	= 8,

	REPLY_SLEEP_OK				= 20,
	REPLY_SLEEP_REJECT			= 21,
	REPLY_WAKEUP_OK				= 22,
	REPLY_WAKEUP_REJECT			= 23,
	REPLY_POWEROFF_OK			= 24,
	REPLY_POWEROFF_REJECT		= 25
};

enum
{
	IDLE_LOW_CLOCK_CHANGE_STATE	= 0xa, //일반동작시..
	REQUEST_LOW_CLOCK			= 0xb, // low clcok으로 내릴거니깐 app에서 low clock관련 작업을 하라고 요청
	REPLY_WORK_FINISH	 		= 0xc  // app에서 처리 끝났다고 알려줌..
};

enum
{
	AUDIOTYPE_NONE = -1,
	AUDIOTYPE_MP3,
	AUDIOTYPE_WMA,			
	AUDIOTYPE_OGG
};

enum
{
	MUSIC_PLAYBACK_NONE		= 0,
	MUSIC_PLAYBACK_OTHERS	= 1,
	MUSIC_PLAYBACK_MP3_128	= 2,
	MUSIC_PLAYBACK_OGG		= 3,
};

enum
{
	MUSIC_EQ_NORMAL			= 0,
	MUSIC_EQ_ROCK			= 1,
	MUSIC_EQ_JAZZ			= 2,
	MUSIC_EQ_POP			= 3,
	MUSIC_EQ_CLASSIC		= 4,
	MUSIC_EQ_LIVE			= 5,
	MUSIC_EQ_UBASS			= 6,
	MUSIC_EQ_DANCE			= 7,
};

struct proc_dir_entry *proc_root_fp	= NULL;
struct proc_dir_entry *proc_freq_fp	= NULL;
struct proc_dir_entry *proc_eq_fp	= NULL;
struct proc_dir_entry *proc_step_fp	= NULL;
struct proc_dir_entry *proc_clock_fp= NULL;
struct proc_dir_entry *proc_debug_fp= NULL;
struct proc_dir_entry *proc_sd_state_fp= NULL;
struct proc_dir_entry *proc_board_version_fp= NULL;
struct proc_dir_entry *proc_poweroff_fp= NULL;

char proc_freq_str[11] 	= { 0,};
char proc_eq_str[11] 	= { 0,};
char proc_step_str[11] 	= { 0,};
char proc_clock_str[11] 	= { 0,};
char proc_debug_str[11] 	= { 0,};
char proc_sd_state_str[11] 	= { 0,};
char proc_board_version_str[11] 	= { 0,};
char proc_poweroff_str[11] 	= { 0,};

static void dcc_loop_timer_worker (void);
static void change_to_low_clock(void);
void start_sleep_timer(void);
static void go_into_sleep_mode(void);
void start_change_low_clock_schedd(void);
void resume_gpio(void);
void delete_and_restart_timer(void);
int do_changing_clock(unsigned int level);		//janged_clk

extern void s3c6410_pm_all_gpio_to_input(void);
#if NEW_SLEEP_WAY
extern int pm_enter_sleep_mode(int cur_mode, int times);
#else
extern int pm_enter_sleep_mode(int cur_mode);
#endif

extern void set_poweroff_time(int time);
extern unsigned char get_hold_key_state(void);
extern unsigned char get_cur_epd_mode(void);
extern void audio_enable_for_pm(void);
extern void audio_disable_for_pm(void);
extern void gpio_input_test (void);
extern int epson_epd_reboot(int gray_val);//gray_val, -1:framebuffer가공하지 않으므로 이전이미지 그대로 나옴, 0x00~0xff: 0xff는 white, 0x00은 black, 중간값은 gray값을 화면에 나타나게함. 
extern void init_gpio(void);
extern void epson_suspend(void);
extern void epson_resume(void);
extern void wm8731_poweroff(void);
extern void wm8731_poweron(void);
extern void elisa_save_core_register(void);
extern void restart_lowclock_battery_check_timer(int clkState);
extern void delete_battery_check_timer(void);
extern int get_cur_battery_level(void);
extern void start_battery_check_forever(void);
extern int reinit_port_for_pm(void);
extern unsigned long get_chip_id(void);
extern int get_elice_board_version(void);
extern unsigned char get_battery_state_with_sleep(void);
extern void set_poweroff_key_chk_loop_time(unsigned short value);

#ifndef CURRENT_OPTIMIZE
static const unsigned int pll_mps_table[][5] = {
	#if 0
	{266, 	9,	2, 	3,	4}, /* 99 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ // 3.72 -> 257
	{266, 	5,	2, 	0,	5}, /* Music Play mp3 : 320kbps OK, OGG Q10 OK, WMA 320kbps OK*/
	{266, 	5,	2, 	2,	6}, /* Music Play mp3 : 128kbps OK*/
	{266, 	6, 	2, 	0,	2}, /* 133 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{400, 	6, 	1, 	0,	1}, /* step 1 ARM:(400 / 1)=400 MHz ,HCLKx2:266 HCLK: 133*/  //Console OK
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김
	{333, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김
	#else
	{266, 	9,	2, 	0,	4}, /* 99 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ // 3.72 -> 257
	{266, 	3,	1, 	0,	3}, /* Music Play mp3 : 320kbps OK, OGG Q10 OK, WMA 320kbps OK*/
	{266, 	3,	1, 	0,	5}, /* Music Play mp3 : 128kbps OK*/
	{266, 	3, 	1, 	0,	1}, /* 133 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{400, 	6, 	1, 	0,	1}, /* step 1 ARM:(400 / 1)=400 MHz ,HCLKx2:266 HCLK: 133*/  //Console OK
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김
	{333, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김

	#endif
	//7
/*	{MDIV, PDIV, SDIV, ARM RATIO, HCLKx2 RATIO } */
};
#else
static const unsigned int pll_mps_table[][5] = {
#if 0
	{266, 	12,	2, 	7,	7}, /* 99 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ // 3.72 -> 257
	{266, 	5,	2, 	3,	5}, /* Music Play mp3 : 320kbps, WMA 320kbps */
	{266, 	5,	2, 	7,	7}, /* Music Play mp3 :  ~ 128kbps  */
	{266, 	5, 	2, 	1,	5}, /* Music Play OGG : Q1 ~ Q10 */
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{400, 	6, 	1, 	0,	1}, /* step 1 ARM:(400 / 1)=400 MHz ,HCLKx2:266 HCLK: 133*/  //Console OK
	//{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{266, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김
	{333, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //667MHz
	//7
#else
	{266, 	12,	2, 	7,	7}, /* 99 step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ // 3.72 -> 257
	{266, 	3,	1, 	0,	3}, /* Music Play mp3 : 320kbps, WMA 320kbps */
	{266, 	3,	1, 	0,	4}, /* Music Play mp3 :  ~ 128kbps  */
	{266, 	3, 	1, 	0,	1}, /* Music Play OGG : Q1 ~ Q10 */
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{400, 	6, 	1, 	0,	1}, /* step 1 ARM:(400 / 1)=400 MHz ,HCLKx2:266 HCLK: 133*/  //Console OK
	//{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{266, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ 
	{533, 	6, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //poweroff 때문에 남김
	{333, 	3, 	1, 	0,	1}, /* step 0 ARM:(533 / 1)=533 MHz ,HCLKx2:266 HCLK: 133*/ //667MHz
#endif
/*	{MDIV, PDIV, SDIV, ARM RATIO, HCLKx2 RATIO } */
};
#endif

static unsigned char exitSleepMode = 0;
static unsigned char lowClockCheckTimer = 0;
static unsigned char checkLowClockState = 0;
static unsigned char loopTimerCount = 0;
static unsigned char superClockFlag = 0;
static unsigned char debugFlag = 0;
static unsigned char changeToHighClock = 0;
static unsigned char cur_play_state = 0;


static unsigned char cur_eq_mode = MUSIC_EQ_NORMAL;
static unsigned char cur_clock_state = CLOCK_MOD_HIGH;
static unsigned char willbeLowClock = 0;
static unsigned char doNotChangeToLowClock = 0;
static unsigned char keyPressedFlag = 0;
static int appIsAlive = 0;
static int silentClockChange = 0;
static int gointoLowClock = 0;
static int curClockState = IDLE_LOW_CLOCK_CHANGE_STATE;
static int sleepCount = 0;
static int curSleepTimes = 0;  //몇번째 sleep에서 깨어났는지 알기위한 flag 1: 다음 sleep준비 2: powerOff
static int firmwareUpgradeFlag = 0;
static int curUSBConnectState = DISABLE;
static int curUSBDetectState = USB_DISCONNECT;
static int curSleepState = SLEEP_NONE;
static int curSleepMode = SLEEP_NONE;
static unsigned char readyToChangeClock = 0;
static unsigned char wakeupFromSleep = 0;
static int previous_dcc_step_for_tty  = 6;
static unsigned char stillWorking = 0;
static elisa_dcc_info dcc_info;
static unsigned char musicLowClockwaitCount = 0;
static unsigned char sendPowerOffKey = 0;

/* Timer 관련 정의들 */
struct timer_list dcc_timer;
struct timer_list dcc_sleep_timer;
struct timer_list dcc_loop_timer;

static DEFINE_MUTEX(dcc_mutex);
static DEFINE_MUTEX(dcc_timer_mutex);

#ifdef JANGED_MODIFY
static DEFINE_MUTEX(readytochange);
static DEFINE_MUTEX(changetolowclock);
static DEFINE_MUTEX(dochangeclock);
#endif

static DECLARE_WORK(dcc_work, (void *)&change_to_low_clock);
static DECLARE_WORK(dcc_sleep_work, (void *)&go_into_sleep_mode);
static DECLARE_WORK(dcc_loop_work, (void *)&dcc_loop_timer_worker);

//janged add start
#ifdef JANGED_MODIFY
void set_ready_to_change_clock(int input)
{
//	dccdbg("##%d mutex lock ##\n", __LINE__);
	mutex_lock(&readytochange);
	readyToChangeClock = input;
//	dccdbg("##%d mutex unlock##\n", __LINE__);
	mutex_unlock(&readytochange);
}

int get_ready_to_change_clock(void)
{
	int ret;
//	dccdbg("##%d mutex lock##\n", __LINE__);
	mutex_lock(&readytochange);
	ret = readyToChangeClock;
//	dccdbg("##%d mutex unlock#\n", __LINE__);
	mutex_unlock(&readytochange);

	return ret;
}

EXPORT_SYMBOL(get_ready_to_change_clock);
#endif

//janged add end

void dcc_timer_function(unsigned long data)
{
	schedule_work(&dcc_work);
	mod_timer(&dcc_timer, jiffies + REWORK_PERIOD);
}

void dcc_sleep_timer_function(unsigned long data)
{
	schedule_work(&dcc_sleep_work);
	mod_timer(&dcc_sleep_timer, jiffies + SLEEP_WAIT_PRIOD);
}

void delete_loop_timer_worker(void)
{
	loopTimerCount = 0;
	checkLowClockState = 0;
	lowClockCheckTimer = 0;
	del_timer_sync(&dcc_loop_timer);
}

void dcc_loop_timer_function(unsigned long data)
{
	schedule_work(&dcc_loop_work);
	mod_timer(&dcc_loop_timer, jiffies + LOOP_REWORK_PERIOD);
}

void start_loop_count_worker(void)
{
	init_timer(&dcc_loop_timer);
 	dcc_loop_timer.function = dcc_loop_timer_function;
 	dcc_loop_timer.expires = jiffies + LOOP_REWORK_PERIOD;
	add_timer(&dcc_loop_timer);
}

void usb_otg_clk_disable(void)
{
	mdelay(10);
	__raw_writel(0x0003f379, S3C_PWR_CFG);
	curUSBConnectState = DISABLE;
}
EXPORT_SYMBOL(usb_otg_clk_disable);

void usb_otg_clk_enable(void)
{
	curUSBConnectState = ENABLE;
	__raw_writel(0x0001f379, S3C_PWR_CFG);
	mdelay(50);
}
EXPORT_SYMBOL(usb_otg_clk_enable);

int get_cur_clock_state(void)
{
	return cur_clock_state; 
}
EXPORT_SYMBOL(get_cur_clock_state);

void set_cur_sleep_times(int way)
{
	curSleepTimes = way;
}
EXPORT_SYMBOL(set_cur_sleep_times);

int get_cur_sleep_times(void)
{
	return curSleepTimes;
}
EXPORT_SYMBOL(get_cur_sleep_times);

int get_cur_flow_state(void)
{			
	return	appIsAlive;
}
EXPORT_SYMBOL(get_cur_flow_state);

static int set_freq_divider(unsigned int clk_source , unsigned int val)
{
	unsigned int tmp;

	if (clk_source == ARM_RATIO)
	{
		if(val > MAX_APLL_RATIO) 
		{
			printk(KERN_ERR "Freq divider value(APLL_RATIO) is out of spec\n");
			printk(KERN_ERR "APLL_RATIO : 0~7\n");
			return -EINVAL;
		}
		tmp = __raw_readl(ARM_CLK_DIV)&~ARM_DIV_MASK;
		tmp |= val<<ARM_DIV_RATIO_BIT;
		__raw_writel(tmp, ARM_CLK_DIV);
	} 
	else if (clk_source == HCLK_RATIO)
	{
		if(val > MAX_HCLK2_RATIO) 
		{
        	printk(KERN_ERR "Freq divider value is out of spec\n");
            printk(KERN_ERR "HCLK_PostDivider RATIO : 0,1 \n");
            return -EINVAL;
        }
        tmp =  __raw_readl(ARM_CLK_DIV) & ~HCLK_DIV_MASK;
        tmp |= (val<<HCLK_DIV_RATIO_BIT);
        __raw_writel(tmp, ARM_CLK_DIV);
	}
	else if (clk_source == PCLK_RATIO)
	{
		if(val > MAX_HCLK2_RATIO) 
		{
        	printk(KERN_ERR "Freq divider value is out of spec\n");
            printk(KERN_ERR "HCLK_PostDivider RATIO : 0,1 \n");
            return -EINVAL;
        }
        tmp =  __raw_readl(ARM_CLK_DIV) & ~ARM_DIV_MASK;
        tmp |= (val<<12);
        __raw_writel(tmp, ARM_CLK_DIV);
	} 
	else if (clk_source == CLKJPEG_RATIO)
	{
		if(val > MAX_HCLK2_RATIO) 
		{
        	printk(KERN_ERR "Freq divider value is out of spec\n");
            printk(KERN_ERR "HCLK_PostDivider RATIO : 0,1 \n");
            return -EINVAL;
        }
        tmp =  __raw_readl(ARM_CLK_DIV) & ~ARM_DIV_MASK;
        tmp |= (val<<24);
        __raw_writel(tmp, ARM_CLK_DIV);
	} 
	else if (clk_source == MFC_RATIO)
	{
		if(val > MAX_HCLK2_RATIO) 
		{
        	printk(KERN_ERR "Freq divider value is out of spec\n");
            printk(KERN_ERR "HCLK_PostDivider RATIO : 0,1 \n");
            return -EINVAL;
        }
        tmp =  __raw_readl(ARM_CLK_DIV) & ~ARM_DIV_MASK;
        tmp |= (val<<28);
        __raw_writel(tmp, ARM_CLK_DIV);
	} 
	
	else if (clk_source == CLKSECUR_RATIO)
	{
		if(val > MAX_HCLK2_RATIO) 
		{
        	printk(KERN_ERR "Freq divider value is out of spec\n");
            printk(KERN_ERR "HCLK_PostDivider RATIO : 0,1 \n");
            return -EINVAL;
        }
        tmp =  __raw_readl(ARM_CLK_DIV) & ~ARM_DIV_MASK;
        tmp |= (val<<28);
        __raw_writel(tmp, ARM_CLK_DIV);
	} 
	
	else 
	{
		printk(KERN_ERR " It's wrong clock post divider path \n");
		return -EINVAL;
	}
	return 0;
}

void emergency_poweroff(void)
{
	//이때는 정말로 battery가 부족하여 epd를 초기화도 못하는 상태 poweroff 시켜야 한다.
	//speak off.
	s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);
	gpio_set_value(S3C64XX_GPK(3), 0);
	//mute.
	s3c_gpio_cfgpin(S3C64XX_GPK(4), S3C_GPIO_OUTPUT);
	gpio_set_value(S3C64XX_GPK(4), 1);
	s3c_gpio_cfgpin(S3C64XX_GPK(6), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S3C64XX_GPK(6), S3C_GPIO_PULL_DOWN);
}

unsigned char get_cur_music_play_state(void)
{
	return cur_play_state;
}
EXPORT_SYMBOL(get_cur_music_play_state);

int get_cur_freq_level(void)
{
	return previous_dcc_step_for_tty;
}

int set_plls(unsigned int freq_level)
{
	unsigned int val;
	static unsigned char initFlag = 1;
	static unsigned int cur_freq_level = 6;

	if (debugFlag && freq_level < 6)
		return 1;

	if (get_cur_freq_level() == freq_level || (!appIsAlive && keyPressedFlag))
	{
		dccdbg("**** same freq level %d *****  \n",freq_level);
		keyPressedFlag = 0;
		return 1;
	}
	
#if 1
	//음악 재생중에 굳이 clock을 높일 필요가 있나??
	if (cur_play_state == 1 && (freq_level == 6 || freq_level == 8) && !keyPressedFlag)
		return 1;
#endif
		
	keyPressedFlag = 0;
	
	/* 이미 low clock으로 내리기 위해 진행중인 상태에서 High clock으로 변경해야 하는 경우
	   high clock이 우선순위가 높기 때문에 low clock으로 떨어뜨리지 말아야 한다. */
	if (willbeLowClock == 1 && freq_level > 3 && cur_play_state == 0)
	{
		doNotChangeToLowClock = 1;
	}

	/* 교모한 타이밍에 low clock과 high clock 명령이 겹쳤을때... */
	#ifdef JANGED_MODIFY
	if ( (doNotChangeToLowClock && freq_level < 4) || (get_ready_to_change_clock() == 0 && !initFlag && freq_level == 0) )
	#else
	if ( (doNotChangeToLowClock && freq_level < 4) || (readyToChangeClock == 0 && !initFlag && freq_level == 0) )
	#endif
	{
		dccdbg("#### do not change to freq level %d ####  \n",freq_level);
		doNotChangeToLowClock = 0; 
		delete_and_restart_timer();		
		willbeLowClock = 0;
		return 1;	
	}

	if (stillWorking)
	{
		delete_and_restart_timer();
		if (freq_level >= 6 && cur_freq_level < 6 && !silentClockChange)
		{
			freq_level = 6;			
		}
		else 
			return 1;

	}

	if (freq_level < NUMBER_OF_STEP )
	{
		stillWorking = 1;
		cur_freq_level = freq_level;

#ifdef CURRENT_OPTIMIZE
#ifndef CLOCK_TEST
		if (cur_freq_level >= 6 && previous_dcc_step_for_tty < 6)
		{
			s3c_gpio_cfgpin(S3C64XX_GPK(0), S3C_GPIO_SFN(1));
			gpio_set_value(S3C64XX_GPK(0), PIN_HIGH);
			udelay(3);	
		}
		else if (cur_freq_level == 1 && previous_dcc_step_for_tty == 2)
		{
			s3c_gpio_cfgpin(S3C64XX_GPK(0), S3C_GPIO_SFN(1));
			gpio_set_value(S3C64XX_GPK(0), PIN_HIGH);
			udelay(3);	
		}
#endif
#endif
		/* freq_level = 7 : 
		   low battery상태에서 poweroff를 하기 위해 클락을 올리는 경우 */
		if (cur_freq_level == 7)
		{
			exitSleepMode = 1;
			printk("Low battery state !! freq level %d !!!! \n", cur_freq_level);
//			dccdbg("## %d mutex lock ##\n", __LINE__);
			mutex_lock(&dcc_timer_mutex);
			cur_clock_state = CLOCK_MOD_HIGH;
			del_timer_sync(&dcc_timer);
			del_timer_sync(&dcc_sleep_timer);
			delete_loop_timer_worker();
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(0);
			#else
			readyToChangeClock = 0;
			#endif
			wakeupFromSleep = 0;
			curClockState = IDLE_LOW_CLOCK_CHANGE_STATE;
			gointoLowClock = 0;
			sleepCount = 0;
			mutex_unlock(&dcc_timer_mutex);
//			dccdbg("##%d mutex unlock ##\n", __LINE__);
		}

		//최종적으로 여기서 베터리 체크클 한번 더 한다. low battery 상태이면 clock을 내리지 않는다.
		if (get_cur_battery_level() == 99 && cur_freq_level <= 1)
		{
			static int loopCount = 0;

			loopCount ++;
			printk("Do not change to low clock for low battery state !!!!!!!!!!!\n");
			restart_lowclock_battery_check_timer(2);
			if (loopCount >= 3)
			{
				loopCount = 0;
				emergency_poweroff();
			}
		
			stillWorking = 0;
			return 1;
		}
	
		__raw_writel(0x4b1, S3C_APLL_LOCK);
		__raw_writel(0x4b1, S3C_MPLL_LOCK);
		
		printk("Change freq level %d range  \n",cur_freq_level);

		#if 0
		if (cur_freq_level == 8)
		{
			if(cur_play_state == 0)
			{
				previous_dcc_step_for_tty  = cur_freq_level;
				set_freq_divider(ARM_RATIO,  pll_mps_table[cur_freq_level][ARM_RATIO]);

				set_freq_divider(HCLK_RATIO, pll_mps_table[6][HCLK_RATIO]);
			}
		}
		else
		{
			if(cur_play_state == 1)
			{
				if((cur_freq_level == MUSIC_PLAYBACK_OTHERS) || 
					(cur_freq_level == MUSIC_PLAYBACK_MP3_128) || 
					(cur_freq_level == MUSIC_PLAYBACK_OGG))
				{
					previous_dcc_step_for_tty  = cur_freq_level;
					set_freq_divider(ARM_RATIO,  pll_mps_table[cur_freq_level][ARM_RATIO]);

					set_freq_divider(HCLK_RATIO, pll_mps_table[cur_freq_level][HCLK_RATIO]);
				}
			}
			else
			{
				previous_dcc_step_for_tty  = cur_freq_level;
				set_freq_divider(ARM_RATIO,  pll_mps_table[cur_freq_level][ARM_RATIO]);

				set_freq_divider(HCLK_RATIO, pll_mps_table[cur_freq_level][HCLK_RATIO]);
			}
		}
		#else
		previous_dcc_step_for_tty  = cur_freq_level;
		//set_freq_divider(ARM_RATIO,  pll_mps_table[cur_freq_level][ARM_RATIO]);
		if (cur_freq_level == 8)
			set_freq_divider(HCLK_RATIO, pll_mps_table[6][HCLK_RATIO]);
		else			
			set_freq_divider(HCLK_RATIO, pll_mps_table[cur_freq_level][HCLK_RATIO]);
		#endif
/*		
		err = set_freq_divider(CLKJPEG_RATIO, pll_mps_table[freq_level][HCLK_RATIO]);
		err = set_freq_divider(MFC_RATIO, pll_mps_table[freq_level][HCLK_RATIO]);
		err = set_freq_divider(PCLK_RATIO, pll_mps_table[freq_level][HCLK_RATIO]);
*/	

		//음악을 듣고 있을때는 APLL 및 MPLL을 변경하면 안됨(음이 끊긴다)
		if (cur_play_state == 0 || (initFlag && cur_freq_level >= 6))
		{
			val = PLL_CALC_VAL(pll_mps_table[cur_freq_level][MDIV],\
		    	                pll_mps_table[cur_freq_level][PDIV],\
		        	            pll_mps_table[cur_freq_level][SDIV]);
			
			__raw_writel(val, ARM_PLL_CON);

			//MAX Clock을 쓰더라도 MPLL은 Max로 쓰면 안됨.
			if (cur_freq_level == 8)
			{
				val = PLL_CALC_VAL(pll_mps_table[6][MDIV],\
				                    pll_mps_table[6][PDIV],\
		        		            pll_mps_table[6][SDIV]);
				
			}
			__raw_writel(val, ARM_MPLL_CON);
		}

		if (cur_freq_level == 6 || cur_freq_level == 8)
		{
			cur_clock_state = CLOCK_MOD_HIGH;
			set_poweroff_key_chk_loop_time(40);
		
			if (!initFlag)
				restart_lowclock_battery_check_timer(1);			
		}
		else if (cur_freq_level < 6)
		{
			cur_clock_state = CLOCK_MOD_LOW;
			restart_lowclock_battery_check_timer(0);		
			set_poweroff_key_chk_loop_time(5);
		}
		
#ifdef CURRENT_OPTIMIZE
#ifndef CLOCK_TEST
		if (cur_freq_level == 0 || cur_freq_level == 2)
		{
			s3c_gpio_cfgpin(S3C64XX_GPK(0), S3C_GPIO_SFN(1));
			gpio_set_value(S3C64XX_GPK(0), PIN_LOW);
		}
#endif
#endif	

		initFlag = 0;
		stillWorking = 0;
		return 0;
	}
	else
	{
		printk("It's wrong DFS %d range  \n",cur_freq_level);
		return 1;
	}
		
	return 0;
}

static void dcc_loop_timer_worker (void)
{
	dccdbg("### (%d) (%d) (%d) (%d) ###\n", checkLowClockState, loopTimerCount, sleepCount, get_cur_freq_level());
	dccdbg("### (%d) (%d) (%d)      ###\n\n", cur_play_state, cur_clock_state, lowClockCheckTimer);	

	if (curSleepState == REQUEST_POWEROFF)
		return;

	if (checkLowClockState == 1)
	{
		loopTimerCount ++;
		if (loopTimerCount >= 50)
		{
			printk("#### dcc_loop_timer_worker() go into low clock with super clock!!!! ####\n");
			delete_and_restart_timer();
			gointoLowClock = 1;	
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(1);
			#else
			readyToChangeClock = 1;
			#endif
			superClockFlag = 0;
			loopTimerCount = 0;
		}		
	}
	else
	{
		loopTimerCount = 0;
	}

	if (cur_clock_state == CLOCK_MOD_HIGH && curUSBConnectState == DISABLE 
		&& !firmwareUpgradeFlag && !checkLowClockState)
	{
		lowClockCheckTimer ++;
		if (lowClockCheckTimer >= 50)
		{
			printk("#### dcc_loop_timer_worker() go into low clcok !!!! ####\n");
			delete_and_restart_timer();
			gointoLowClock = 1;	
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(1);
			#else
			readyToChangeClock = 1;
			#endif

			lowClockCheckTimer = 0;
		}
	}
	else
	{
		lowClockCheckTimer = 0;
	}
}

spinlock_t eplllock; 	/* Initialize */ //janged_clk
unsigned long eplllock_flags;
int do_changing_clock(unsigned int level)
{	

	static unsigned int prev_level = 99;

//	dccdbg("##%d spin lock ##\n",__LINE__);
	spin_lock(&eplllock);
#if defined(CONFIG_SUPPORT_FACTORY) || defined(CONFIG_LIBERTAS)
	if (level < 6 )
	{
		dccdbg("##%d spin unlock ##\n",__LINE__);		
		spin_unlock(&eplllock);
		return 0;		//janged_clk
	}
#endif

	if (level == 6)
		level = 8;

	//level = 8;

	if ( (prev_level == level))
	{
		spin_unlock(&eplllock);
	//	dccdbg("##%d spin unlock ##\n",__LINE__);		
		return 0;
	}

	if (stillWorking && level < 6)
	{
		delete_and_restart_timer();
		spin_unlock(&eplllock);
	//	dccdbg("##%d spin unlock ##\n",__LINE__);		
		return 0;
	}


	
	lock_kernel();
	prev_level = level;
	if (set_plls(prev_level) == 1)
		prev_level = 99;
	unlock_kernel();
	spin_unlock(&eplllock);
	//dccdbg("##%d spin unlock ##\n",__LINE__);		
	return 1;
}
EXPORT_SYMBOL(do_changing_clock);

void key_pressed(void)
{
	int ret = 0;
	if (get_hold_key_state() == 0 )
	{
		#ifdef JANGED_MODIFY
		dccdbg("Key pressed !!!\n");

		keyPressedFlag = 1;
		musicLowClockwaitCount = 0;
		if (get_cur_freq_level() > 4 )
		{
			
			ret = do_changing_clock(8);
		}
		else
		{
			ret = do_changing_clock(6);
		}

		if(ret)
		{
			delete_and_restart_timer();
			set_ready_to_change_clock(1);
		}
		#else
		keyPressedFlag = 1;
		musicLowClockwaitCount = 0;
		if (get_cur_freq_level() > 4 )
		{
			do_changing_clock(8);
		}
		else
		{
			do_changing_clock(6);
		}
		delete_and_restart_timer();
		readyToChangeClock = 1;
		#endif
	}
	else
	{
		dccdbg("HOLD Key enabled !!!\n");
	}
	#ifdef JANGED_MODIFY
	if(ret)
	{
		lowClockCheckTimer = 0;
	}
	#else
	lowClockCheckTimer = 0;
	#endif
}
EXPORT_SYMBOL(key_pressed);

void interrupt_externel_event(void)
{
	keyPressedFlag = 1;
	do_changing_clock(6);
	delete_and_restart_timer();	
	lowClockCheckTimer = 0;
}
EXPORT_SYMBOL(interrupt_externel_event);

unsigned char get_cur_sleep_mode(void)
{
	return curSleepMode;
}
EXPORT_SYMBOL(get_cur_sleep_mode);

void start_sleep_timer(void)
{
   	init_timer(&dcc_sleep_timer);
   	dcc_sleep_timer.function = dcc_sleep_timer_function;
   	dcc_sleep_timer.expires = jiffies + SLEEP_WAIT_PRIOD;
   	add_timer(&dcc_sleep_timer);
}

void start_change_low_clock_schedd(void)
{
    init_timer(&dcc_timer);
    dcc_timer.function = dcc_timer_function;
    dcc_timer.expires = jiffies+REWORK_PERIOD;
    add_timer(&dcc_timer);
}

void delete_and_restart_timer(void)
{
	if (firmwareUpgradeFlag == 1)
	{
		return;
	}

//	dccdbg("##%d mutex lock ##\n",__LINE__);
	mutex_lock(&dcc_timer_mutex);
	
	curClockState = IDLE_LOW_CLOCK_CHANGE_STATE;
	gointoLowClock = 0;	
	willbeLowClock = 0;
	del_timer_sync(&dcc_timer);
	start_change_low_clock_schedd();

	mutex_unlock(&dcc_timer_mutex);
//	dccdbg("##%d mutex unlock ##\n",__LINE__);
}
EXPORT_SYMBOL(delete_and_restart_timer);

void delete_all_dcc_timer(void)
{
//	dccdbg("##%d mutex lock ##\n", __LINE__);
	mutex_lock(&dcc_timer_mutex);

	del_timer_sync(&dcc_timer);
	curClockState = IDLE_LOW_CLOCK_CHANGE_STATE;
	gointoLowClock = 0;	
	willbeLowClock = 0;
	del_timer_sync(&dcc_sleep_timer);
	sleepCount = 0;
	
	mutex_unlock(&dcc_timer_mutex);
//	dccdbg("##%d mutex unlock ##\n", __LINE__);
}

void set_readyToChangeClock(int input)
{
	readyToChangeClock = input;
	if (input == 0)
	{
		curClockState = IDLE_LOW_CLOCK_CHANGE_STATE;
		gointoLowClock = 0;
	}
}


EXPORT_SYMBOL(set_readyToChangeClock);

void clear_sleep_count(void)
{
	sleepCount = 0;
}
EXPORT_SYMBOL(clear_sleep_count);

unsigned char get_poweroff_state_in_sleep(void)
{
	return sendPowerOffKey;
}
EXPORT_SYMBOL(get_poweroff_state_in_sleep);

void set_poweroff_state_in_sleep(unsigned char val)
{
	sendPowerOffKey = val;
}
EXPORT_SYMBOL(set_poweroff_state_in_sleep);

static void change_clock_for_music(void)
{
	printk("##  filetype is %d %d eq=%d ##\n", dcc_info.music_file_type, dcc_info.music_rate, cur_eq_mode);
	if (dcc_info.music_file_type == AUDIOTYPE_MP3)
	{
		dccdbg("##  filetype is mp3 %d  ##\n", dcc_info.music_rate);

		//EQ 먹인 상태면 Clock을 올리자...
		if (cur_eq_mode > 0)
		{
			//mp3, 128kbps이면 
			if (dcc_info.music_rate <= 128)
			{
				do_changing_clock(MUSIC_PLAYBACK_OTHERS);
			}
			else
			{
				do_changing_clock(MUSIC_PLAYBACK_OGG);
			}
			
		}
		else
		{
			//mp3, 128kbps이면 
			if (dcc_info.music_rate <= 128)
			{
				do_changing_clock(MUSIC_PLAYBACK_MP3_128);
			}
			else
			{
				do_changing_clock(MUSIC_PLAYBACK_OTHERS);	
			}
		}
	}
	else if (dcc_info.music_file_type == AUDIOTYPE_OGG)
	{
		do_changing_clock(MUSIC_PLAYBACK_OGG);
	}
	else if (dcc_info.music_file_type == 100)
	{
		dccdbg("######### now recording %d########\n", dcc_info.music_rate);
		do_changing_clock(6);
	}	
	else
	{
		if (cur_eq_mode > 0)
		{
			do_changing_clock(MUSIC_PLAYBACK_OGG);
		}
		else
			do_changing_clock(MUSIC_PLAYBACK_OTHERS);
	}
	dccdbg("clock for music done !! \n");
}

static void change_to_low_clock(void)
{			
	unsigned char wasInWaitState = 0;

	if (changeToHighClock)
	{
		do_changing_clock(6);
		changeToHighClock = 0;
		delete_and_restart_timer();
		dccdbg("\n## need to change High clock ########\n");
		return;
	}

	if (get_cur_sleep_mode() == SLEEP_ON)
		return;

	if (cur_play_state == 0)
	{
		#ifdef JANGED_MODIFY
		if ((superClockFlag == 0 
			&& get_ready_to_change_clock() == 1 
			&& firmwareUpgradeFlag == 0 
			&& curUSBConnectState == DISABLE) 
			|| (wakeupFromSleep && curUSBConnectState == DISABLE ) )
		#else
		if ((superClockFlag == 0 && readyToChangeClock == 1 && firmwareUpgradeFlag == 0 
			&& curUSBConnectState == DISABLE) 
			|| (wakeupFromSleep && curUSBConnectState == DISABLE ) )
		#endif
		{
			if (curClockState != REPLY_WORK_FINISH)
				curClockState = REQUEST_LOW_CLOCK; 

			if (gointoLowClock == 1)
			{
				gointoLowClock = 0;
				wasInWaitState = 1;
			}
			else
			{
				if (!silentClockChange)
				{
					dccdbg("## to check again!! %d##\n", curClockState);
					return;
				}
			}

			//EPD가 sleep mode로 진입한 후에 clock을 내려야 한다.
			if (get_cur_epd_mode() == 0)
			{
				dccdbg("## Current epd mode is not sleep state, so try to check again!! ##\n");
				#ifdef JANGED_MODIFY
				set_ready_to_change_clock(1);
				#else
				readyToChangeClock = 1;
				#endif
				delete_and_restart_timer();
				if (wasInWaitState == 1)
					gointoLowClock = 1;
				return;
			}

			
			willbeLowClock = 1;
			/* dccdbg("##  Go into low frequency  ##\n"); */
//			printk("dcc_timer_mutex lock on %d ++\n", __LINE__);
			mutex_lock(&dcc_timer_mutex);
			del_timer_sync(&dcc_timer);
			if (!silentClockChange && get_hold_key_state() == 0)
			{
				sleepCount = 0;
			}
//			dccdbg("##%d mutex unlock ##\n", __LINE__);
			mutex_unlock(&dcc_timer_mutex);
			wakeupFromSleep = 0;
			gointoLowClock = 0;
			if (doNotChangeToLowClock == 0)
			{
				mdelay(200);
				del_timer_sync(&dcc_sleep_timer);
				//printk("######### Change to low frequency #########\n");
				do_changing_clock(0); //Low clock
#ifndef DO_NOT_ENTER_SLEEPMODE		
				start_sleep_timer();
#endif
				#ifdef JANGED_MODIFY
				set_ready_to_change_clock(0);
				#else
				readyToChangeClock = 0;
				#endif
			}
			else
			{
				printk("######### Do not Change to low frequency #########\n");
				delete_and_restart_timer();
				#ifdef JANGED_MODIFY
				set_ready_to_change_clock(1);
				#else
				readyToChangeClock = 1;
				#endif
			}
			silentClockChange = 0;
			musicLowClockwaitCount = 0;
			doNotChangeToLowClock = 0; 
			willbeLowClock = 0;
			checkLowClockState = 0;
			lowClockCheckTimer = 0;
		}
		else
		{
			if (firmwareUpgradeFlag == 1)
				dccdbg("now firmware updating !!!\n");			
			else if (superClockFlag == 1)
			{
				//1분동안 계속되면 무조건 clock을 내린다.
				dccdbg("waitting for recieving super clock finish !!!\n");	
				checkLowClockState = 1;
			}
/*			
			else
			{
				dccdbg("waitting for recieving update finish message (%d)!!!\n", curUSBConnectState);
			}
*/			
		}
	}
	//음악이 재생중이면 클락을 조금만 낮춘다.
	else
	{
		#ifdef JANGED_MODIFY
		if (superClockFlag == 0 && get_ready_to_change_clock() == 1 && 
			firmwareUpgradeFlag == 0 && curUSBConnectState == DISABLE)
		{

		#else
		if (superClockFlag == 0 && readyToChangeClock == 1 && 
			firmwareUpgradeFlag == 0 && curUSBConnectState == DISABLE)
		{
		#endif
			if (musicLowClockwaitCount == 0)
			{
				musicLowClockwaitCount = 1;
				dccdbg("#### waitCount frequency ####\n");
				return;
			}
			else
			{
				dccdbg("#### Change to Music low frequency ####\n");
				musicLowClockwaitCount = 0;
			}

			change_clock_for_music();
//			dccdbg("##%d mutex lock ##\n", __LINE__);
			mutex_lock(&dcc_timer_mutex);
			del_timer_sync(&dcc_timer);
			del_timer_sync(&dcc_sleep_timer);
			sleepCount = 0;
			mutex_unlock(&dcc_timer_mutex);
//			dccdbg("##%d mutex unlock ##\n", __LINE__);
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(0);
			#else
			readyToChangeClock = 0;
			#endif
			checkLowClockState = 0;
			lowClockCheckTimer = 0;
		}
		else
		{
			if (firmwareUpgradeFlag == 1)
				dccdbg("now firmware updating !!!\n");
			else if (superClockFlag == 1)
			{
				dccdbg("[Kernel] waitting for recieving super clock finish !!!\n");		
				checkLowClockState = 1;
			}
/*			
			else
				dccdbg("waitting for recieving update finish message 2 !!!\n");			
*/				
		}		
	}
}

static void go_into_sleep_mode(void)
{
#if SUPPORT_SLEEP_MODE
	if (changeToHighClock)
	{
		do_changing_clock(6);
		changeToHighClock = 0;
		delete_and_restart_timer();
		printk("\n########### need to change High clock #\n");
		return;
	}

	if (cur_clock_state == CLOCK_MOD_LOW && curUSBDetectState == USB_DISCONNECT)
	{
		sleepCount ++;
		if (sleepCount >= FORCE_SLEEP_TIME)
		{
			if (cur_play_state == 0 && curUSBConnectState == DISABLE && firmwareUpgradeFlag == 0)
			{
				do_changing_clock(6);
				//sleep mode로 진입하라고 app에게 알려준다.
				curSleepState = REQUEST_SLEEP_ENTER_1;
				curSleepMode = SLEEP_ON;
#if NEW_SLEEP_WAY
//				dccdbg("##%d mutex lock ##\n", __LINE__);
				mutex_lock(&dcc_timer_mutex);
				del_timer_sync(&dcc_sleep_timer);
				del_timer_sync(&dcc_timer);
				sleepCount = 0;
				mutex_unlock(&dcc_timer_mutex);
//				dccdbg("##%d mutex unlock ##\n", __LINE__);
#else				
				del_timer_sync(&dcc_sleep_timer);
				sleepCount = 0;
				pm_enter_sleep_mode(PM_SUSPEND_MEM);
				
				epson_epd_reboot(-1);//기존 이미지 그대로 나오게함.		
				#ifdef JANGED_MODIFY
				set_ready_to_change_clock(1);
				#else
				readyToChangeClock = 1;
				#endif

				delete_and_restart_timer();
#endif
			}
			else
			{
//				dccdbg("##%d mutex lock ##\n", __LINE__);
				mutex_lock(&dcc_timer_mutex);
				del_timer_sync(&dcc_sleep_timer);
				sleepCount = 0;
				mutex_unlock(&dcc_timer_mutex);
//				dccdbg("##%d mutex unlock ##\n",__LINE__);
				printk("\n[Kernel] Skip the sleep mode !!!\n");			
			}
		}
	}
	else
	{
		if ((!silentClockChange && get_hold_key_state() == 0) || curUSBDetectState == USB_CONNECT)
			sleepCount = 0;
	}
#else
	del_timer_sync(&dcc_sleep_timer);
#endif//SUPPORT_SLEEP_MODE
}

int elisa_dcc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t elisa_dcc_read(struct file *filp, char *buf, size_t count,
			loff_t *f_pos)
{
	return 0;
}

ssize_t elisa_dcc_write(struct file *filp, const char *buf, size_t count,
			loff_t *f_pos)
{
	return 0;
}


int elisa_dcc_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int err, size;
	static unsigned char initFlag = 1;

#ifdef CONFIG_SUPPORT_FACTORY
	return 0;	
#endif//SUPPORT_FACTORY
//	dccdbg("##%d mutex lock ##\n", __LINE__);
	mutex_lock(&dcc_mutex);
	size = _IOC_SIZE(cmd);

	switch(cmd) 
	{
		case DCC_MUSIC_PLAYBACK_ON:
			dccdbg("Get DCC_MUSIC_PLAYBACK_ON command !!\n");
			cur_play_state = 1;
			audio_enable_for_pm();
			err=copy_from_user((void *) &dcc_info, (const void *) arg, size);
			if(err != 0)
			{
				cur_play_state = 0;
//				dccdbg("##%d mutex unlock ##\n", __LINE__);
				mutex_unlock(&dcc_mutex);
				return err;
			}
			dccdbg("\nGet DCC_MUSIC_PLAYBACK_ON command !!\n");

			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(1);
			#else
			readyToChangeClock = 1;
			#endif
			break;
		case DCC_MUSIC_PLAYBACK_OFF:
			#if 1
			dccdbg("\nGet DCC_MUSIC_PLAYBACK_OFF command !!\n");
			//audio_disable_for_pm();			//janged_clk codec off 하는 경우에 만 ... 하도록 
			delete_and_restart_timer();
			cur_play_state = 0;
			dccdbg("Get DCC_MUSIC_PLAYBACK_OFF command !!\n");
			#endif
			break;
		case DCC_UPDATE_FINISH:
			dccdbg("Get DCC_UPDATE_FINISH !!\n");
			if (cur_clock_state == CLOCK_MOD_HIGH)
			{
				delete_and_restart_timer();
			}
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(1);
			#else
			readyToChangeClock = 1;
			#endif
			break;
		case DCC_GET_STATUS:
			dcc_info.device_id = get_chip_id();
			err=copy_to_user((void *) arg, (const void *) &dcc_info, (unsigned long) size);
			break;
		case DCC_SET_STATUS:	

			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(0);
			#else
			readyToChangeClock = 0;
			#endif

			appIsAlive = 1;
			if (firmwareUpgradeFlag == 1)
			{
				break;
			}

			if (cur_clock_state != CLOCK_MOD_HIGH)
			{
				err=copy_from_user((void *) &dcc_info, (const void *) arg, size);
				if(err != 0)
				{
//					dccdbg("##%d mutex unlock ##\n", __LINE__);
					mutex_unlock(&dcc_mutex);
					return err;
				}
				cur_clock_state = dcc_info.curr_mode;
				if (cur_clock_state == CLOCK_MOD_HIGH)
				{
#if 1
					//음악 playback중에는 pll을 올리지 않는다.
					if (get_cur_freq_level() != MUSIC_PLAYBACK_OTHERS &&
						get_cur_freq_level() != MUSIC_PLAYBACK_MP3_128)
					{
						do_changing_clock(8);
					}
					else if (get_cur_freq_level() == MUSIC_PLAYBACK_OTHERS ||
							 get_cur_freq_level() == MUSIC_PLAYBACK_MP3_128)
					{
						cur_clock_state = CLOCK_MOD_LOW;
					}
#else
					do_changing_clock(8);

#endif
				}
			}
			else
			{
				if (get_cur_freq_level() == 6)
				{
					do_changing_clock(8);
				}
				//flow쪽에 계속해서 이벤트가 일어나고 있는것이다.
			}
				
			dccdbg("Flow event -> curr_mode %d\n", cur_clock_state);
			//Clock이 이미 low로 떨어진 상태에서는 timer를 다시 돌릴 필요가 없다.
			if (cur_clock_state == CLOCK_MOD_HIGH)
				delete_and_restart_timer();
		
			start_battery_check_forever();
			break;
		case DCC_FIRMWARE_UPGRADE:
			if (firmwareUpgradeFlag == 1)
				break;

			if (cur_clock_state == CLOCK_MOD_LOW)
			{
				do_changing_clock(6);
			}
			
			dccdbg("Get DCC_FIRMWARE_UPGRADE command !!\n");
//			dccdbg("##%d mutex lock ##\n", __LINE__);
			mutex_lock(&dcc_timer_mutex);
			delete_loop_timer_worker();
			del_timer_sync(&dcc_sleep_timer);
			del_timer_sync(&dcc_timer);
			delete_battery_check_timer();
//			dccdbg("##%d mutex unlock ##\n", __LINE__);
			mutex_unlock(&dcc_timer_mutex);
			firmwareUpgradeFlag = 1;
			break;
		case DCC_WAIT_CHANGE_LOW_CLOCK:
			superClockFlag = 1;
			checkLowClockState = 1;
			dccdbg("\nGet DCC_WAIT_CHANGE_LOWCLOCK command !!!!\n");
			break;
		case DCC_DO_CHANGE_LOW_CLOCK:
			superClockFlag = 0;
			checkLowClockState = 0;
			loopTimerCount = 0;
			if (cur_clock_state == CLOCK_MOD_HIGH)
			{
				delete_and_restart_timer();
			}
			
			dccdbg("\nGet DCC_DO_CHANGE_LOW_CLOCK command !!!!\n");
			break;
			
		default:
			break;
	}

//	dccdbg("##%d mutex unlock ##\n",__LINE__);
	
	if (initFlag)
	{
		initFlag = 0;
		elisa_save_core_register();
		s3c_gpio_cfgpin(S3C64XX_GPK(0), S3C_GPIO_SFN(1));
		gpio_set_value(S3C64XX_GPK(0), PIN_HIGH);
		udelay(2);			
	}
	mutex_unlock(&dcc_mutex);
	return 0;
}

int elisa_dcc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations elisa_dcc_fops =
{
	.owner		= THIS_MODULE,
	.read		= elisa_dcc_read,
	.write		= elisa_dcc_write,
	.ioctl		= elisa_dcc_ioctl,
	.open		= elisa_dcc_open,
	.release	= elisa_dcc_release,
};

int read_proc_sleep(char *page, char **start, off_t off,
					int count, int *eof, void *data_unused)
{
	char *buf;

	buf = page;

	buf += sprintf(buf, "%d\n", curSleepState);

	return buf - page;
}

int write_proc_sleep(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, tmp;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	tmp = simple_strtoul(realdata, NULL, 10);

	dccdbg("sleep value %d \n",tmp);
	
	return count;
}

int write_proc_poweroff_time(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, time;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	time = simple_strtoul(realdata, NULL, 10);

	printk("poewroff time value %d \n", time);
	
	set_poweroff_time(time);

	return count;
}

int read_proc_clock(char *page, char **start, off_t off,
					int count, int *eof, void *data_unused)
{
	char *buf;

	buf = page;

	buf += sprintf(buf, "%d\n", curClockState);

	return buf - page;
}

int write_proc_clock(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, reply;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	reply = simple_strtoul(realdata, NULL, 10);

	dccdbg("Recieve value %d from app\n",reply);
	
	if (reply == REPLY_WORK_FINISH)
	{
		gointoLowClock = 1;
		curClockState = REPLY_WORK_FINISH;
	}

	return count;
}

int write_proc_gointosleep(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, reply;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	reply = simple_strtoul(realdata, NULL, 10);

	dccdbg("proc_gotosleep value %d \n",reply);

#if NEW_SLEEP_WAY
	switch(reply)
	{
		case REPLY_SLEEP_OK:
		{
			curSleepMode = SLEEP_ON;	
#if SUPPORT_SLEEP_MODE
			if (exitSleepMode == 1)
			{
				set_cur_sleep_times(2);
			}
			else
			{
				//최종적으로 sleep mode로 진입시키는 함수
				gointoLowClock = 0;
				pm_enter_sleep_mode(PM_SUSPEND_MEM, 0); 
				gointoLowClock = 0;	
				//1시간 대기후 wakeup된 상태이면.
				if (get_cur_sleep_times() == 1)
				{
					mdelay(10);
					//하얀색 화면 뿌려주고 다시 sleep으로 들어가면
					epson_epd_reboot(0xff);//흰색 이미지 나오게함.		
					mdelay(4000);
					if (exitSleepMode == 0)
						pm_enter_sleep_mode(PM_SUSPEND_MEM, 1); 
					else
						set_cur_sleep_times(2);
				}
				mdelay(10);
				epson_epd_reboot(-1);	
				mdelay(500);
			}
#endif
#if 1
			//깨어 났을때 3으로 세팅해야 은동이가 APP에서 nand mounting시킨다.
			if (get_cur_sleep_times() == 2)// && !get_battery_state_with_sleep()) //PowerOff
			{
				curSleepState = REQUEST_WAKEUP;
//				msleep(1000);
//				dccdbg("%d mutex lock ##\n",__LINE__);
				mutex_lock(&dcc_timer_mutex);
				del_timer_sync(&dcc_timer);
				del_timer_sync(&dcc_sleep_timer);
				sleepCount = 0;
				mutex_unlock(&dcc_timer_mutex);									
//				dccdbg("##%d mutex unlock ##\n",__LINE__);
				//curSleepState = REQUEST_POWEROFF;
				sendPowerOffKey = 1;
			}
			else
			{
				/* APP에 Wakeup 하라고 요청. */
				curSleepState = REQUEST_WAKEUP; 
			}
#endif
			gointoLowClock = 0;
			curSleepMode = SLEEP_NONE;	
			exitSleepMode = 0;
		}
		break;
		
		case REPLY_SLEEP_REJECT:
		{
			curSleepMode = SLEEP_NONE;	
			#ifdef JANGED_MODIFY
			set_ready_to_change_clock(1);
			#else
			readyToChangeClock = 1;
			#endif

			wakeupFromSleep = 1;
			delete_and_restart_timer();
		}
		break;
		
		case USB_CONNECT:
		{
			interrupt_externel_event();
			curUSBDetectState = USB_CONNECT;
			/* 내가 APP에게 Sleep Mode를 진입하라고 요청해 놓은 상태이면... */
			if (curSleepState == REQUEST_SLEEP_ENTER_1)
			{
				curSleepMode = SLEEP_NONE;
				delete_and_restart_timer();	
				#ifdef JANGED_MODIFY
				set_ready_to_change_clock(1);
				#else
				readyToChangeClock = 1;
				#endif

			}
		}
		break;
		
		case USB_DISCONNECT:
		{
			curUSBDetectState = USB_DISCONNECT;
			if (cur_clock_state == CLOCK_MOD_HIGH)
				mdelay(50);
			else
				udelay(50);
			
			interrupt_externel_event();
			/* USB를 제거 하구 나서 battery check를 해줘야 한다. */
			restart_lowclock_battery_check_timer(3);
		}
		break;

		case USB_SELECTED_CONNECTION_WAY:
		{
			do_changing_clock(6);
			curUSBDetectState = USB_SELECTED_CONNECTION_WAY;
		}
		break;
			
		case REPLY_WAKEUP_OK:
		case REPLY_WAKEUP_REJECT:
		case REPLY_POWEROFF_OK:
		case REPLY_POWEROFF_REJECT:
		{
		}
		break;
		default:
		break;
	}
#endif

	return count;
}

int write_proc_eq(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, reply;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	reply = simple_strtoul(realdata, NULL, 10);

	printk("proc_eq_mode value %d --\n",reply);
	cur_eq_mode = reply;

	return count;
}

/* low clock일때 console이 깨지므로 Debugging하기힘들다... 1이면 debug모드 0 이면 non debug 
   default value = 0 */
int write_proc_debug(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	int len, reply;
	char *realdata;

	realdata = (char *) data;

	if(copy_from_user(realdata, buffer, count))
		return -EFAULT;

	realdata[count] = '\0';
	len = strlen(realdata);
	if(realdata[len - 1] == '\n')
		realdata[--len] = 0;
	reply = simple_strtoul(realdata, NULL, 10);

	dccdbg("proc_debug value %d \n",reply);
	debugFlag = reply;

	return count;
}

extern int sd_mmc_status;

int read_proc_sd_state(char *page, char **start, off_t off,
					int count, int *eof, void *data_unused)
{
	char *buf;

	buf = page;

	buf += sprintf(buf, "%d\n", sd_mmc_status);

	return buf - page;
}

int read_proc_board_version(char *page, char **start, off_t off,
					int count, int *eof, void *data_unused)
{
	char *buf;

	buf = page;

	buf += sprintf(buf, "%d\n", get_elice_board_version());

	return buf - page;
}


int elisa_dcc_init(void)
{
	int result;

	result = register_chrdev(DEV_MAJOR, DEV_NAME, &elisa_dcc_fops);

	spin_lock_init(&eplllock);
	if(result < 0)
		return result;

	/* proc table 등록 */
	proc_root_fp = proc_mkdir("dccfs", 0);

	proc_freq_fp = create_proc_entry("sleepstate", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_freq_fp) {
		proc_freq_fp->data = proc_freq_str;
		proc_freq_fp->read_proc = read_proc_sleep;
		proc_freq_fp->write_proc = write_proc_sleep;
	}

	proc_eq_fp = create_proc_entry("eqmode", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_eq_fp) {
		proc_eq_fp->data = proc_eq_str;
		proc_eq_fp->read_proc = NULL;
		proc_eq_fp->write_proc = write_proc_eq;
	}
	
	proc_step_fp = create_proc_entry("gointosleep", S_IFREG | S_IWUSR, proc_root_fp);
	if(proc_step_fp) {
		proc_step_fp->data = proc_step_str;
		proc_step_fp->read_proc = NULL;
		proc_step_fp->write_proc= write_proc_gointosleep;
	}

	proc_poweroff_fp = create_proc_entry("powerofftime", S_IFREG | S_IWUSR, proc_root_fp);
	if(proc_poweroff_fp) {
		proc_poweroff_fp->data = proc_poweroff_str;
		proc_poweroff_fp->read_proc = NULL;
		proc_poweroff_fp->write_proc= write_proc_poweroff_time;
	}

	proc_clock_fp = create_proc_entry("lowclock", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_clock_fp) {
		proc_clock_fp->data = proc_clock_str;
		proc_clock_fp->read_proc = read_proc_clock;
		proc_clock_fp->write_proc = write_proc_clock;
	}

	proc_debug_fp = create_proc_entry("debug", S_IFREG | S_IWUSR, proc_root_fp);
	if(proc_debug_fp) {
		proc_debug_fp->data = proc_debug_str;
		proc_debug_fp->read_proc = NULL;
		proc_debug_fp->write_proc= write_proc_debug;
	}
	
	proc_sd_state_fp = create_proc_entry("sd_state", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_sd_state_fp) {
		proc_sd_state_fp->data = proc_sd_state_str;
		proc_sd_state_fp->read_proc = read_proc_sd_state;
		proc_sd_state_fp->write_proc= NULL;
	}
	
	proc_board_version_fp = create_proc_entry("board_version", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_board_version_fp) {
		proc_board_version_fp->data = proc_board_version_str;
		proc_board_version_fp->read_proc = read_proc_board_version;
		proc_board_version_fp->write_proc= NULL;
	}
	
	/* 변수 및 구조체 초기화 */
	dcc_info.music_file_type = AUDIOTYPE_MP3;
	dcc_info.music_rate = 320;
	cur_clock_state = dcc_info.curr_mode = CLOCK_MOD_HIGH;
	cur_play_state = 0;

	/* USB OTG PWR을 무조건 OFF시켜버린다. */
	usb_otg_clk_disable();

	/* 주기적(10초마다)으로 Check할 수 있는 Timer를 구동시킨다. */
	start_loop_count_worker();

#ifdef REQUEST_HARDWARE
	enable_all_clock();
#endif
	
	printk("[Elisa] Installed iriver Dynamic Clock Change Driver \n");
	return 0;
}

void elisa_dcc_exit(void)
{
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
	remove_proc_entry("powerofftime", proc_root_fp);
	remove_proc_entry("eqmode", proc_root_fp);
	remove_proc_entry("sleepstate", proc_root_fp);
	remove_proc_entry("gointosleep", proc_root_fp);
	remove_proc_entry("lowclock", proc_root_fp);
	remove_proc_entry("debug", proc_root_fp);
	remove_proc_entry("sd_state", proc_root_fp);
	remove_proc_entry("board_version", proc_root_fp);
	remove_proc_entry("dccfs", 0);
	delete_loop_timer_worker();
}

void force_high_clock(void)
{
	//음악 playback중에는 pll을 올리지 않는다.
	if (curSleepMode == SLEEP_NONE && cur_clock_state == CLOCK_MOD_LOW && 
		get_cur_freq_level() != MUSIC_PLAYBACK_OTHERS &&
		get_cur_freq_level() != MUSIC_PLAYBACK_MP3_128 && 
		get_cur_freq_level() != MUSIC_PLAYBACK_OGG)
	{
		delete_and_restart_timer();
		silentClockChange = 1;
		do_changing_clock(6);
#ifndef DO_NOT_ENTER_SLEEPMODE		
		if (curUSBDetectState == USB_DISCONNECT)
			sleepCount ++;
#endif
		#ifdef JANGED_MODIFY
		set_ready_to_change_clock(1);
		#else
		readyToChangeClock = 1;
		#endif

		dccdbg("\n##  Silence Clock change sleep count=%d ## \n", sleepCount);
	}
}
EXPORT_SYMBOL(force_high_clock);

module_init(elisa_dcc_init);
module_exit(elisa_dcc_exit);

MODULE_DESCRIPTION("iriver Dynamic Clock Change Driver");
MODULE_AUTHOR("Tom.Lee, <tom.lee@iriver.com>");
