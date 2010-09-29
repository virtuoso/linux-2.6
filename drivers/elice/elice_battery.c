/* 
 * ebook_battery.c - iRiver ebook battery driver for s3c6410
 *
 * Original author: Tom <tom@iriver.com>
 * 
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>

#define BATTERY_DEV_NAME	"battery"
#define BATTERY_DEV_MAJOR	231

#define DTLK_MINOR  0
#define GET_CUR_BATTERY_STATE 0x01
#define ADC_READ_COUNT 10

#define BATT_LEVEL_LOW		0x2e6  //cut off 
#define BATT_LEVEL_BLANK 	0x308
#define BATT_LEVEL_1		0x325  //1칸	
#define BATT_LEVEL_2 		0x348  //2칸
#define BATT_LEVEL_FULL 	0x36a  //3칸

#define LEVEL_3			3
#define LEVEL_2			2
#define LEVEL_1			1
#define LEVEL_BLANK 	0
#define LEVEL_LOW		99

#define LOW_CLOCK	`	0
#define HIGH_CLOCK		1

#define LOW_HZ			(168)  //low clock일때 168정도가 10초정도 이다.
#define LOW_HZ_HALF		(84)  //low clock일때 168정도가 5초정도 이다.
#define LOW_HZ_HALF2	(34)  // 1.5초

//#define BATTERY_TEST
#ifdef BATTERY_TEST
#define CHECK_PERIOD_LEVEL123				(HZ*2)      
#define CHECK_PERIOD_WITH_LOWCLOCK_LEVEL123	(LOW_HZ*1) 
#else
/* 30초 마다 한번식 체크 */
#define CHECK_PERIOD_LEVEL123					(HZ*30)    // High Clock 상태  
#define CHECK_PERIOD_WITH_LOWCLOCK_LEVEL123		(LOW_HZ*3) // Low Clock 상태 
#endif

//#define BATTERY_DEBUG
#ifdef BATTERY_DEBUG
#define bddbg(X...) do { printk("<kernel> %s(): ", __func__); printk(X); } while(0)
#else
#define bddbg(format, arg...) do {} while (0)
#endif

void battery_check_worker(void);
extern int get_cur_freq_level(void);
extern unsigned char get_cur_music_play_state(void);
extern int read_adc_ch(int ch);
extern int do_changing_clock(unsigned int level);		//janged
extern int get_cur_sleep_mode(void);

static int ebook_battery_major;
struct timer_list battery_timer;

static DECLARE_WORK(battery_work, (void *)&battery_check_worker);
static int curTmpBatteryLevel = LEVEL_3;
static int curBatteryLevel = LEVEL_3;
static unsigned char curClockStateforbattery = HIGH_CLOCK;
static unsigned int CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF*1;
static unsigned int CHECK_PERIOD_LEVEL0 = HZ*1;
static unsigned char doBatteryCheckForever = 0;


void battery_timer_function(unsigned long data)
{
	schedule_work(&battery_work);

	if (curClockStateforbattery == HIGH_CLOCK)
	{
		if (curBatteryLevel == LEVEL_LOW)
		{
			mod_timer(&battery_timer, jiffies + HZ);
		}
		else if (curBatteryLevel >= LEVEL_1)
		{
			mod_timer(&battery_timer, jiffies + CHECK_PERIOD_LEVEL123);
		}
		else
		{
			mod_timer(&battery_timer, jiffies + CHECK_PERIOD_LEVEL0);
		}
	}
	else
	{
		if (curBatteryLevel == LEVEL_LOW)
		{
			mod_timer(&battery_timer, jiffies + 10);
		}
		else if (curBatteryLevel >= LEVEL_1)
		{
			mod_timer(&battery_timer, jiffies + CHECK_PERIOD_WITH_LOWCLOCK_LEVEL123);
		}
		else
		{
			mod_timer(&battery_timer, jiffies + CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0);
		}
	}
}

void init_battery_timer(int clkState)
{
   	init_timer(&battery_timer);
  	battery_timer.function = battery_timer_function;
	if (clkState == HIGH_CLOCK) //high clock
	{
		if (curBatteryLevel == LEVEL_LOW)
		{
   			battery_timer.expires = jiffies + HZ;
		}
		else if (curBatteryLevel >= LEVEL_1)
		{
   			battery_timer.expires = jiffies + CHECK_PERIOD_LEVEL123;
		}
		else
		{
   			battery_timer.expires = jiffies + CHECK_PERIOD_LEVEL0;
		}
	}
	else //low clock
	{
		if (curBatteryLevel == LEVEL_LOW)
		{
   			battery_timer.expires = jiffies + HZ;
		}
		else if (curBatteryLevel >= LEVEL_1)
		{
   			battery_timer.expires = jiffies + CHECK_PERIOD_WITH_LOWCLOCK_LEVEL123;
		}
		else
		{
   			battery_timer.expires = jiffies + CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0;
		}
	}
   	add_timer(&battery_timer);	
}

void start_battery_check_forever(void)
{
	doBatteryCheckForever = 1;
}

void restart_lowclock_battery_check_timer(int clkState)
{
    unsigned int adc, adc_total, i;
    int try = ADC_READ_COUNT;
	unsigned char curBattery = 3;
	int curClkState = 0;

	if (curBatteryLevel == LEVEL_LOW && clkState != 2)
		return;

	curClkState = clkState;

	if (clkState == 2 || clkState == 3)
		clkState = 1;

    adc_total = 0;

	//ADC값 읽을때 처음 2~3번은 항상 높다... 그래서 그 값은 무시...
	for (i=0; i <= 1; i++)
        read_adc_ch(6);

	while(try-- > 0)
    {
        adc = read_adc_ch(6);
        adc_total += adc;
    }

    adc = adc_total / ADC_READ_COUNT;

	bddbg("##  adc = 0x%x  ##\n", adc);
	if (adc > BATT_LEVEL_2)
		curBattery = LEVEL_3;
	else if (adc <= BATT_LEVEL_2 && adc > BATT_LEVEL_1)
		curBattery = LEVEL_2;
	else if (adc <= BATT_LEVEL_1 && adc > BATT_LEVEL_BLANK)
		curBattery = LEVEL_1;
	else if (adc <= BATT_LEVEL_BLANK && adc > BATT_LEVEL_LOW)
	{
		curBattery = LEVEL_BLANK;
		if (adc <= BATT_LEVEL_LOW + 0x6)
		{
			CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF;
			CHECK_PERIOD_LEVEL0 = HZ*2;
		}
	}
	else
	{
/*		
		if (doBatteryCheckForever == 0)
		{
			curClockState = clkState;
			del_timer_sync(&battery_timer);
			init_battery_timer(clkState);			
			return;
		}
		do_changing_clock(7);
		curBatteryLevel = curBattery = LEVEL_LOW;
		return;
*/		
		CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF2;
		CHECK_PERIOD_LEVEL0 = HZ*2;
		curBatteryLevel = curBattery = LEVEL_BLANK;
	}

	//USB를 뺐었을 경우에는 저장해서 APP에게 알려줘야 한다.
	if (curClkState == 3)
		curBatteryLevel = curBattery;

	curClockStateforbattery = clkState;
	del_timer_sync(&battery_timer);
	init_battery_timer(clkState);
}
EXPORT_SYMBOL_GPL(restart_lowclock_battery_check_timer);

void delete_battery_check_timer(void)
{
	del_timer_sync(&battery_timer);
}

void battery_check_worker(void)
{
    unsigned int adc, adc_total, i;
    int try = ADC_READ_COUNT;
	static unsigned char sameLevelCount = 0;
	static int prevBatteryLevel = LEVEL_LOW;
	static unsigned char initFlag = 1;
	static unsigned char setCount = 0;	
	int checkCount = 8;

	if (curBatteryLevel == LEVEL_LOW || (doBatteryCheckForever == 0 && !initFlag))
	{
		return;
	}
	
    adc_total = 0;

	//ADC값 읽을때 처음 2~3번은 항상 높다... 그래서 그 값은 무시...
	for (i=0; i <= 1; i++)
        read_adc_ch(6);

	while(try-- > 0)
    {
        adc = read_adc_ch(6);
        adc_total += adc;
    }

    adc = adc_total / ADC_READ_COUNT;

	if (get_cur_music_play_state() == 1)
		checkCount = 30;
	else
		checkCount = 15;

	//bddbg("## adc=0x%x cCount=%d sCount=%d ##\n", adc, checkCount, sameLevelCount);
	if (curClockStateforbattery == HIGH_CLOCK)
	{
		if (adc > BATT_LEVEL_2)
		{
			curTmpBatteryLevel = LEVEL_3;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_2 && adc > BATT_LEVEL_1)
		{
			curTmpBatteryLevel = LEVEL_2;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_1 && adc > BATT_LEVEL_BLANK)
		{
			curTmpBatteryLevel = LEVEL_1;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_BLANK && adc > BATT_LEVEL_LOW)
		{
			curTmpBatteryLevel = LEVEL_BLANK;
			if (adc <= BATT_LEVEL_LOW + 0x6)
			{
				CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF;
				CHECK_PERIOD_LEVEL0 = HZ*2;
				checkCount = 5;
			}
			setCount = 0;
			bddbg("##  battery state %d adc=0x%x  ##\n",curTmpBatteryLevel , adc);
		}
		else
		{
			if (!initFlag && get_cur_sleep_mode() == 0)
			{
				if (setCount > 1)
				{
					do_changing_clock(7);
					curBatteryLevel = curTmpBatteryLevel = LEVEL_LOW;
					del_timer_sync(&battery_timer);
				}
				setCount ++;
			}
			else 
			{
				setCount = 0;
				curBatteryLevel = curTmpBatteryLevel = LEVEL_BLANK;
			}
			
			bddbg("##  Low battery level %d adc=0x%x  ##\n", curBatteryLevel, adc);
			return;
		}
	}
	else
	{
		if (adc > BATT_LEVEL_2)
		{
			curTmpBatteryLevel = LEVEL_3;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_2 && adc > BATT_LEVEL_1)
		{
			curTmpBatteryLevel = LEVEL_2;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_1 && adc > BATT_LEVEL_BLANK)
		{
			curTmpBatteryLevel = LEVEL_1;
			setCount = 0;
		}
		else if (adc <= BATT_LEVEL_BLANK && adc > BATT_LEVEL_LOW + 0x8)
		{
			curTmpBatteryLevel = LEVEL_BLANK;
			CHECK_PERIOD_LEVEL0 = HZ*2;
			CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ;
			if (adc <= BATT_LEVEL_LOW + 0xd)
			{
				if (get_cur_music_play_state() == 1)
				{
					checkCount = 10;
				}
				else
				{
					CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF;
					checkCount = 5;
				}
			}
			setCount = 0;
			//bddbg("##  Low clock battery state %d adc=0x%x  ##\n",curTmpBatteryLevel , adc);
			bddbg("## battery s=%d adc=0x%x (%d) (%d) ##\n",curTmpBatteryLevel , adc, CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0, checkCount);
		}
		else
		{
			if (!initFlag && get_cur_sleep_mode() == 0)
			{
				if (setCount > 1)
				{
					do_changing_clock(7);
					curBatteryLevel = curTmpBatteryLevel = LEVEL_LOW;
					del_timer_sync(&battery_timer);
				}
				setCount ++;
			}
			else 
			{
				setCount = 0;
				curBatteryLevel = curTmpBatteryLevel = LEVEL_BLANK;
			}
					
			CHECK_PERIOD_WITH_LOWCLOCK_LEVEL0 = LOW_HZ_HALF2;
			
			bddbg("# battery level %d adc=0x%x sCount=%d #\n", curBatteryLevel, adc, setCount);
			return;
		}		
	}

	if (prevBatteryLevel == curTmpBatteryLevel)
		sameLevelCount ++;
	else
		sameLevelCount = 0;

	if (sameLevelCount >= checkCount)
	{
		bddbg("######### before battery level %d adc=0x%x#########\n", curBatteryLevel, adc);
		curBatteryLevel = curTmpBatteryLevel;
		sameLevelCount = 0;
		bddbg("######### after  battery level %d #########\n", curBatteryLevel);
	}

	if (initFlag)
	{
		curBatteryLevel = curTmpBatteryLevel;
		initFlag = 0;
	}

	prevBatteryLevel = curTmpBatteryLevel;
}

int get_cur_battery_level(void)
{
    return curBatteryLevel;
}

EXPORT_SYMBOL_GPL(get_cur_battery_level);

unsigned int get_cur_battery_adc_value(void)
{
    unsigned int adc, adc_total, i;
    int try = ADC_READ_COUNT;

    adc_total = 0;

	//ADC값 읽을때 처음 2~3번은 항상 높다... 그래서 그 값은 무시...
	for (i=0; i <= 1; i++)
        read_adc_ch(6);

	while(try-- > 0)
    {
        adc = read_adc_ch(6);
        adc_total += adc;
    }

    adc = adc_total / ADC_READ_COUNT;

	bddbg("##  adc = 0x%x  ##\n", adc);

    return adc;
}
EXPORT_SYMBOL_GPL(get_cur_battery_adc_value);

int check_battery_level_for_bootup(int sequence)
{
	if (sequence)
	{
		if (get_cur_battery_adc_value() <= BATT_LEVEL_LOW + 0xb)
			return 1;
		else
			return 0;
	}
	else
	{
		if (get_cur_battery_adc_value() < BATT_LEVEL_LOW + 0x5)
			return 1;
		else
			return 0;		
	}
}
EXPORT_SYMBOL_GPL(check_battery_level_for_bootup);

static int battery_ioctl(struct inode *inode,
		      struct file *file,
		      unsigned int cmd,
		      unsigned long arg)
{

	switch (cmd) 
	{
		case GET_CUR_BATTERY_STATE:
			get_cur_battery_level();
			return 0;

		default:
			return -EINVAL;
	}
}

static int battery_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	switch (iminor(inode)) 
	{
		case DTLK_MINOR:
			return nonseekable_open(inode, file);

		default:
			printk("battery_ioctl error\n");
			return -ENXIO;
	}
}

static ssize_t battery_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	return (ssize_t)get_cur_battery_level();
}

static int battery_release(struct inode *inode, struct file *file)
{
	switch (iminor(inode)) {
	case DTLK_MINOR:
		break;

	default:
		break;
	}
	
	return 0;
}

static const struct file_operations battery_fops =
{
    .owner      = THIS_MODULE,
    .ioctl      = battery_ioctl,
    .open       = battery_open,
    .read       = battery_read,
    .release    = battery_release,
};

static int __init battery_init(void)
{
	ebook_battery_major = register_chrdev(BATTERY_DEV_MAJOR, BATTERY_DEV_NAME, &battery_fops);
	if (ebook_battery_major < 0) {
		printk(KERN_ERR "Elisa Battery driver - cannot register device\n");
		return ebook_battery_major;
	}
	printk("Elisa Battery Driver Init Success\n");
	battery_check_worker();
	init_battery_timer(HIGH_CLOCK);
	return 0;
}

static void __exit battery_cleanup (void)
{
	unregister_chrdev(ebook_battery_major, BATTERY_DEV_NAME);
}


module_init(battery_init);
module_exit(battery_cleanup);

MODULE_LICENSE("GPL");
