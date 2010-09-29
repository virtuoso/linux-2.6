/* drivers/input/keyboard/s3c-keypad.c
 *
 * Driver core for Samsung SoC onboard UARTs.
 *
 * Kim Kyoungil, Copyright (c) 2006-2009 Samsung Electronics
 *      http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>

#include <linux/io.h>
#include <mach/hardware.h>
#include <asm/delay.h>
#include <asm/irq.h>

#include <plat/regs-gpio.h>
#include <plat/regs-keypad.h>

#include "s3c-keypad.h"

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include "../../elice/elice_hw_board_version.h"


//#undef S3C_KEYPAD_DEBUG 
//#define S3C_KEYPAD_DEBUG 
#ifdef S3C_KEYPAD_DEBUG
#define DPRINTK(x...) printk("[kernel] " x)
#else
#define DPRINTK(x...)		/* !!!! */
#endif

#define DEVICE_NAME "s3c-keypad"

#define TRUE 1
#define FALSE 0

/* woong */
/* 태웅 우선 막아 둔다. 사용하는 곳이 없네 */
#if 0
#ifdef CONFIG_PM
static int	wasInSleep = FALSE;
#endif
#endif
extern void key_pressed(void);
extern unsigned char get_cur_sleep_mode(void);
/* end */

static struct timer_list keypad_timer;
static int is_timer_on = FALSE;
static struct clk *keypad_clock;


static int ebook_report_key = true;
extern spinlock_t eplllock;
void ebook_input_report_key(struct input_dev *dev, unsigned int code, int value)
{

	//sleep이 진행 중이라면 ~~
	if (get_cur_sleep_mode() == 5)
		return;

	spin_lock(&eplllock);
	if(ebook_report_key)
	{
		if (value != 2)
			input_report_key(dev, code, value);
	}
	else
		printk("\n>> [kerenl, key] error !!!!! <<\n\n");

	spin_unlock(&eplllock);
}


static int keypad_scan(u32 *keymask_low, u32 *keymask_high)
{
	int i,j = 0;
	u32 cval,rval;

	for (i=0; i<KEYPAD_COLUMNS; i++) 
	{
		cval = readl(key_base+S3C_KEYIFCOL) | KEYCOL_DMASK;
		cval &= ~(1 << i);
		writel(cval, key_base+S3C_KEYIFCOL);

		udelay(KEYPAD_DELAY);

		rval = ~(readl(key_base+S3C_KEYIFROW)) & KEYROW_DMASK;
		
		if ((i*KEYPAD_ROWS) < MAX_KEYMASK_NR)
		{
			*keymask_low |= (rval << (i * KEYPAD_ROWS));
		}
		else 
		{
			*keymask_high |= (rval << (j * KEYPAD_ROWS));
			j = j +1;
		}
	}

	writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);

	return 0;
}

static unsigned prevmask_low = 0, prevmask_high = 0;

static void keypad_timer_handler(unsigned long data)
{
	u32 keymask_low = 0, keymask_high = 0;
	u32 press_mask_low, press_mask_high;
	u32 release_mask_low, release_mask_high;
	int i;
	struct s3c_keypad *pdata = (struct s3c_keypad *)data;
	struct input_dev *dev = pdata->dev;

	keypad_scan(&keymask_low, &keymask_high);

	if (keymask_low != prevmask_low) 
	{
		press_mask_low =
			((keymask_low ^ prevmask_low) & keymask_low); 
		release_mask_low =
			((keymask_low ^ prevmask_low) & prevmask_low); 

		i = 0;
		while (press_mask_low) {
			if (press_mask_low & 1) {
				//input_report_key(dev,pdata->keycodes[i],1);
				ebook_input_report_key(dev,pdata->keycodes[i],1);
				DPRINTK("low Pressed  : %d, i: %d\n", pdata->keycodes[i], i);
			}
			press_mask_low >>= 1;
			i++;

			udelay(KEYPAD_DELAY);//billy
		}

		i = 0;
		while (release_mask_low) {
			if (release_mask_low & 1) {
				//input_report_key(dev,pdata->keycodes[i],0);
				ebook_input_report_key(dev,pdata->keycodes[i],0);
				DPRINTK("low Released : %d, i: %d\n", pdata->keycodes[i], i);
			}
			release_mask_low >>= 1;
			i++;

			udelay(KEYPAD_DELAY);//billy
		}
		prevmask_low = keymask_low;
	}

	if (keymask_high != prevmask_high) 
	{
		press_mask_high = ((keymask_high ^ prevmask_high) & keymask_high); 
		release_mask_high = ((keymask_high ^ prevmask_high) & prevmask_high);

		i = 0;
		while (press_mask_high) 
		{
			if (press_mask_high & 1) {
				//input_report_key(dev,pdata->keycodes[i+MAX_KEYMASK_NR],1);
				ebook_input_report_key(dev,pdata->keycodes[i+MAX_KEYMASK_NR],1);
				DPRINTK("high Pressed  : %d i: %d(+32)\n",pdata->keycodes[i+MAX_KEYMASK_NR],i);
			}
			press_mask_high >>= 1;
			i++;

			//janged for pop noise
			#if 0
			if((pdata->keycodes[i+MAX_KEYMASK_NR] == 7) || (pdata->keycodes[i+MAX_KEYMASK_NR] == 9))
			{
				//mute
				gpio_set_value(S3C64XX_GPK(4), 0);
			}
			#endif
			
			udelay(KEYPAD_DELAY);//billy
		}

		i = 0;
		while (release_mask_high) {
			if (release_mask_high & 1) {
				//input_report_key(dev,pdata->keycodes[i+MAX_KEYMASK_NR],0);
				ebook_input_report_key(dev,pdata->keycodes[i+MAX_KEYMASK_NR],0);
				DPRINTK("high Released : %d i: %d(+32)\n",pdata->keycodes[i+MAX_KEYMASK_NR],i);
			}
			release_mask_high >>= 1;
			i++;

			udelay(KEYPAD_DELAY);//billy
		}
		prevmask_high = keymask_high;
	}

	if (keymask_low | keymask_high) {
		mod_timer(&keypad_timer,jiffies + HZ/10);
	} else {
		//woong : wakeup source
#if 1
		writel(KEYIFCON_INIT, key_base+S3C_KEYIFCON);
#else
		writel(0x1f, key_base+S3C_KEYIFCON);
#endif
		is_timer_on = FALSE;
	}	
}

/* Changed by woong */
#if 1
static irqreturn_t s3c_keypad_isr(int irq, void *dev_id)
{
	/* disable keypad interrupt and schedule for keypad timer handler */
	writel(readl(key_base+S3C_KEYIFCON) & ~(INT_F_EN|INT_R_EN), key_base+S3C_KEYIFCON);

	//woong
	key_pressed();

	keypad_timer.expires = jiffies + (HZ/100);
	if ( is_timer_on == FALSE) {
		add_timer(&keypad_timer);
		is_timer_on = TRUE;
	} else {
		mod_timer(&keypad_timer,keypad_timer.expires);
	}
	/*Clear the keypad interrupt status*/
	writel(KEYIFSTSCLR_CLEAR, key_base+S3C_KEYIFSTSCLR);

	return IRQ_HANDLED;
}
#else
static irqreturn_t s3c_keypad_isr(int irq, void *dev_id)
{
	/* disable keypad interrupt and schedule for keypad timer handler */
	writel(readl(key_base+S3C_KEYIFCON) & ~(INT_F_EN|INT_R_EN), key_base+S3C_KEYIFCON);
	key_pressed();
	keypad_timer_handler(0);
	/*Clear the keypad interrupt status*/
	writel(KEYIFSTSCLR_CLEAR, key_base+S3C_KEYIFSTSCLR);

	return IRQ_HANDLED;

	{
		keypad_timer.expires = jiffies + (HZ/100);
		if ( is_timer_on == FALSE) {
			add_timer(&keypad_timer);
			is_timer_on = TRUE;
		} else {
			mod_timer(&keypad_timer,keypad_timer.expires);
		}
	}

	/*Clear the keypad interrupt status*/
	writel(KEYIFSTSCLR_CLEAR, key_base+S3C_KEYIFSTSCLR);

	return IRQ_HANDLED;
}

#endif

#if 1//billy test
unsigned long ebook_key_data = 0;
extern int get_cur_flow_state(void);

void ebook_external_input_report_key(unsigned int code, int value)
{
	struct s3c_keypad *pdata = (struct s3c_keypad *)ebook_key_data;
	struct input_dev *dev = pdata->dev;
	//DPRINTK("pdata->dev: 0x%x\n", pdata->dev);

	//0:up, 1:down, 2:enter
	
	//input_report_key(dev, pdata->keycodes[code], value);
	ebook_input_report_key(dev, pdata->keycodes[code], value);
	if(code == 22 && value== 0)
	{
		// power long key...
		// flow가 실행되었을때만 key를 종료시켜야 한다.
		//if (get_cur_flow_state() == 1)
		//	ebook_report_key = false;
		printk("\n>> [kerenl, key]check power long key!!! <<\n\n");
	}
	
	DPRINTK("external input_report_key  code: %d, value: %d\n",pdata->keycodes[code], value); //janged power off 관련 해서 막음 
}
EXPORT_SYMBOL_GPL(ebook_external_input_report_key);

extern int get_elice_board_version(void);
#endif//billy


static int __init s3c_keypad_probe(struct platform_device *pdev)
{
	struct resource *res, *keypad_mem, *keypad_irq;
	struct input_dev *input_dev;
	struct s3c_keypad *s3c_keypad;
	int ret, size;
	int key, code;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev,"no memory resource specified\n");
		return -ENOENT;
	}

	size = (res->end - res->start) + 1;

	keypad_mem = request_mem_region(res->start, size, pdev->name);
	if (keypad_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req;
	}

	key_base = ioremap(res->start, size);
	if (key_base == NULL) {
		printk(KERN_ERR "Failed to remap register block\n");
		ret = -ENOMEM;
		goto err_map;
	}

	keypad_clock = clk_get(&pdev->dev, "keypad");
	if (IS_ERR(keypad_clock)) {
		dev_err(&pdev->dev, "failed to find keypad clock source\n");
		ret = PTR_ERR(keypad_clock);
		goto err_clk;
	}

	clk_enable(keypad_clock);
	
	s3c_keypad = kzalloc(sizeof(struct s3c_keypad), GFP_KERNEL);
	input_dev = input_allocate_device();

	if (!s3c_keypad || !input_dev) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	platform_set_drvdata(pdev, s3c_keypad);
	s3c_keypad->dev = input_dev;

	//woong : wakeup source
#if 0
	writel(0x1f, key_base+S3C_KEYIFCON);
#else
	writel(KEYIFCON_INIT, key_base+S3C_KEYIFCON);
#endif

	writel(KEYIFFC_DIV, key_base+S3C_KEYIFFC);

	/* Set GPIO Port for keypad mode and pull-up disable*/
	s3c_setup_keypad_cfg_gpio(KEYPAD_ROWS, KEYPAD_COLUMNS);

	writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);

	/* create and register the input driver */
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_REP, input_dev->evbit);
	s3c_keypad->nr_rows = KEYPAD_ROWS;
	s3c_keypad->no_cols = KEYPAD_COLUMNS;
	s3c_keypad->total_keys = MAX_KEYPAD_NR;

	for(key = 0; key < s3c_keypad->total_keys; key++){
		code = s3c_keypad->keycodes[key] = keypad_keycode[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, input_dev->keybit);
	}

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "s3c-keypad/input0";
	
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	input_dev->keycode = keypad_keycode;

	/* Scan timer init */
	init_timer(&keypad_timer);
	keypad_timer.function = keypad_timer_handler;
	keypad_timer.data = (unsigned long)s3c_keypad;

	//billy add.
	ebook_key_data = (unsigned long)s3c_keypad;

	//billy@temp add.. 나중에 뺄코드. 하드웨어에서 high로 유지할것.
	//*
	//janged test
	//이 if 문 풀지 마세요. 특정 세트에서 이상 키값 올라 옵니다.
//	if(get_elice_board_version() == ELICE_BOARD_WS1)		
	{
		s3c_gpio_cfgpin(S3C64XX_GPK(15), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S3C64XX_GPK(15), S3C_GPIO_PULL_UP);
	}
	//*/
	//end billy@add..

	/* For IRQ_KEYPAD */
	keypad_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (keypad_irq == NULL) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}

	ret = request_irq(keypad_irq->start, s3c_keypad_isr, IRQF_SAMPLE_RANDOM,
		DEVICE_NAME, (void *) pdev);
	if (ret) {
		printk("request_irq failed (IRQ_KEYPAD) !!!\n");
		ret = -EIO;
		goto err_irq;
	}

	ret = input_register_device(input_dev);
	if (ret) {
		printk("Unable to register s3c-keypad input device!!!\n");
		goto out;
	}

	keypad_timer.expires = jiffies + (HZ/10);

	if (is_timer_on == FALSE) {
		add_timer(&keypad_timer);
		is_timer_on = TRUE;
	} else {
		mod_timer(&keypad_timer,keypad_timer.expires);
	}

	printk( DEVICE_NAME " Initialized\n");
	return 0;

out:
	free_irq(keypad_irq->start, input_dev);
	free_irq(keypad_irq->end, input_dev);

err_irq:
	input_free_device(input_dev);
	kfree(s3c_keypad);
	
err_alloc:
	clk_disable(keypad_clock);
	clk_put(keypad_clock);

err_clk:
	iounmap(key_base);

err_map:
	release_resource(keypad_mem);
	kfree(keypad_mem);

err_req:
	return ret;
}

static int s3c_keypad_remove(struct platform_device *pdev)
{
	struct input_dev *input_dev = platform_get_drvdata(pdev);
	writel(KEYIFCON_CLEAR, key_base+S3C_KEYIFCON);

	if(keypad_clock) {
		clk_disable(keypad_clock);
		clk_put(keypad_clock);
		keypad_clock = NULL;
	}

	input_unregister_device(input_dev);
	iounmap(key_base);
	kfree(pdev->dev.platform_data);
	free_irq(IRQ_KEYPAD, (void *) pdev);

	del_timer(&keypad_timer);	
	printk(DEVICE_NAME " Removed.\n");
	return 0;
}

#ifdef CONFIG_PM
static unsigned int keyifcon, keyiffc;

static int s3c_keypad_suspend(struct platform_device *dev, pm_message_t state)
{
	printk("s3c_keypad_suspend\n");
	keyifcon = readl(key_base+S3C_KEYIFCON);
	keyiffc = readl(key_base+S3C_KEYIFFC);

	disable_irq(IRQ_KEYPAD);

	clk_disable(keypad_clock);

	return 0;
}

static int s3c_keypad_resume(struct platform_device *dev)
{
	//woong : Sleep Mode 이후 GPIO초기화 해줘야쥐..
	printk("s3c_keypad_resume\n");
	
	s3c_setup_keypad_cfg_gpio(KEYPAD_ROWS, KEYPAD_COLUMNS);

	s3c_gpio_cfgpin(S3C64XX_GPK(15), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPK(15), S3C_GPIO_PULL_UP);
	
	writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);	

	clk_enable(keypad_clock);

	writel(keyifcon, key_base+S3C_KEYIFCON);
	writel(keyiffc, key_base+S3C_KEYIFFC);

	enable_irq(IRQ_KEYPAD);

	return 0;
}
#else
#define s3c_keypad_suspend NULL
#define s3c_keypad_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver s3c_keypad_driver = {
	.probe		= s3c_keypad_probe,
	.remove		= s3c_keypad_remove,
	.suspend	= s3c_keypad_suspend,
	.resume		= s3c_keypad_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-keypad",
	},
};

static int __init s3c_keypad_init(void)
{
	int ret;

	ret = platform_driver_register(&s3c_keypad_driver);
	
	if(!ret)
	   printk(KERN_INFO "S3C Keypad Driver\n");

	return ret;
}

static void __exit s3c_keypad_exit(void)
{
	platform_driver_unregister(&s3c_keypad_driver);
}

module_init(s3c_keypad_init);
module_exit(s3c_keypad_exit);

MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KeyPad interface for Samsung S3C");
