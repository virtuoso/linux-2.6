/*
	janged add for gpio control....
	for GPX Data only
*/
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include "elisagpio.h"

#define GPIODEV_NAME	"ELISA_GPIO"
#define GPIODEV_MAJOR	245

struct file_operations egpio_fops;
static DEFINE_MUTEX(egio_mutex);

static int egpio_open(struct inode *inod, struct file *file)
{
//	printk("egpio_open : sucessful\n");
	return 0;
}

static int egpio_release(struct inode *inod, struct file *file)
{
//	printk("egpio_release : sucessful\n");
	return 0;
}

static ssize_t egpio_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
//	printk("egpio_read : retruning zero bytes");
	return 0;
}

static ssize_t egpio_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
//	printk("egpio_write : retruning zero bytes");
	return 0;
}

static int read_port_status(egpio_status * status)
{
	switch(status->port_group)
	{
		case GPA:
			if((status->port_num < 0) || (status->port_num) > 7)
			{
				printk("GPA Port Num Range 0~7");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPA(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPA(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPA(status->port_num));

			break;
		case GPB:
			if((status->port_num < 0) || (status->port_num) > 6)
			{
				printk("GPB Port Num Range 0~6");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPB(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPB(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPB(status->port_num));
			break;
		case GPC:
			if((status->port_num < 0) || (status->port_num) > 7)
			{
				printk("GPB Port Num Range 0~7");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPC(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPC(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPC(status->port_num));

			break;
		case GPD:
			if((status->port_num < 0) || (status->port_num) > 4)
			{
				printk("GPD Port Num Range 0~4");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPD(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPD(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPD(status->port_num));

			break;

		case GPE:
			if((status->port_num < 0) || (status->port_num) > 4)
			{
				printk("GPE Port Num Range 0~4");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPE(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPE(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPE(status->port_num));

			break;

		case GPF:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPF Port Num Range 0~15");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPF(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPF(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPF(status->port_num));

			break;

		case GPG:
			if((status->port_num < 0) || (status->port_num) > 6)
			{
				printk("GPG Port Num Range 0~6");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPG(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPG(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPG(status->port_num));

			break;

		case GPH:
			if((status->port_num < 0) || (status->port_num) > 9)
			{
				printk("GPH Port Num Range 0~9");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPH(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPH(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPH(status->port_num));

			break;

		case GPI:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPI Port Num Range 0~15");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPI(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPI(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPI(status->port_num));

			break;

		case GPJ:
			if((status->port_num < 0) || (status->port_num) > 11)
			{
				printk("GPJ Port Num Range 0~11");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPJ(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPJ(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPJ(status->port_num));

			break;

		case GPK:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPK Port Num Range 0~15");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPK(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPK(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPK(status->port_num));

			break;

		case GPL:
			if((status->port_num < 0) || (status->port_num) > 14)
			{
				printk("GPL Port Num Range 0~14");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPL(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPL(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPL(status->port_num));

			break;

		case GPM:
			if((status->port_num < 0) || (status->port_num) > 5)
			{
				printk("GPM Port Num Range 0~5");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPM(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPM(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPM(status->port_num));

			break;

		case GPN:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPN Port Num Range 0~15");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPN(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPN(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPN(status->port_num));

			break;

		case GPO:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPO Port Num Range 0~15");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPO(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPO(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPO(status->port_num));

			break;

		case GPP:
			if((status->port_num < 0) || (status->port_num) > 14)
			{
				printk("GPP Port Num Range 0~14");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPP(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPP(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPP(status->port_num));

			break;

		case GPQ:
			if((status->port_num < 0) || (status->port_num) > 8)
			{
				printk("GPQ Port Num Range 0~8");
				return 0; 
			}
			status ->value = gpio_get_value(S3C64XX_GPQ(status->port_num));
//			status->direction	= s3c_gpio_get_cfg(S3C64XX_GPQ(status->port_num));
//			status->pull_updown = s3c_gpio_getpull(S3C64XX_GPQ(status->port_num));
			break;

		case WM8731:
			status->value = 100;
		break;

		default:
		break;
	}
	return 1;
}

extern void wm8731_poweron(void);
extern void wm8731_poweroff(void);

static int write_port_status(egpio_status * status)
{
	switch(status->port_group)
	{
		case GPA:
			if((status->port_num < 0) || (status->port_num) > 7)
			{
				printk("GPA Port Num Range 0~7");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPA(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPA(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPA(status->port_num), status->value);
			break;
		case GPB:
			if((status->port_num < 0) || (status->port_num) > 6)
			{
				printk("GPB Port Num Range 0~6");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPB(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPB(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPB(status->port_num), status->value);
			break;
		case GPC:
			if((status->port_num < 0) || (status->port_num) > 7)
			{
				printk("GPB Port Num Range 0~7");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPC(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPC(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPC(status->port_num), status->value);
			break;
		case GPD:
			if((status->port_num < 0) || (status->port_num) > 4)
			{
				printk("GPD Port Num Range 0~4");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPD(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPD(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPD(status->port_num), status->value);
			break;

		case GPE:
			if((status->port_num < 0) || (status->port_num) > 4)
			{
				printk("GPE Port Num Range 0~4");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPE(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPE(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPE(status->port_num), status->value);
			break;

		case GPF:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPF Port Num Range 0~15");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPF(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPF(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPF(status->port_num), status->value);
			break;

		case GPG:
			if((status->port_num < 0) || (status->port_num) > 6)
			{
				printk("GPG Port Num Range 0~6");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPG(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPG(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPG(status->port_num), status->value);
			break;

		case GPH:
			if((status->port_num < 0) || (status->port_num) > 9)
			{
				printk("GPH Port Num Range 0~9");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPH(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPH(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPH(status->port_num), status->value);
			break;

		case GPI:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPI Port Num Range 0~15");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPI(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPI(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPI(status->port_num), status->value);
			break;

		case GPJ:
			if((status->port_num < 0) || (status->port_num) > 11)
			{
				printk("GPJ Port Num Range 0~11");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPJ(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPJ(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPJ(status->port_num), status->value);
			break;

		case GPK:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPK Port Num Range 0~15");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPK(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPK(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPK(status->port_num), status->value);
			break;

		case GPL:
			if((status->port_num < 0) || (status->port_num) > 14)
			{
				printk("GPL Port Num Range 0~14");
				return 0; 
			}

//			s3c_gpio_cfgpin(S3C64XX_GPL(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPL(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPL(status->port_num), status->value);
			break;

		case GPM:
			if((status->port_num < 0) || (status->port_num) > 5)
			{
				printk("GPM Port Num Range 0~5");
				return 0; 
			}

//			s3c_gpio_cfgpin(S3C64XX_GPM(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPM(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPM(status->port_num), status->value);
			break;

		case GPN:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPN Port Num Range 0~15");
				return 0; 
			}

//			s3c_gpio_cfgpin(S3C64XX_GPN(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPN(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPN(status->port_num), status->value);
			break;

		case GPO:
			if((status->port_num < 0) || (status->port_num) > 15)
			{
				printk("GPO Port Num Range 0~15");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPO(status->port_num), status->direction);
	//		s3c_gpio_setpull(S3C64XX_GPO(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPO(status->port_num), status->value);
			break;

		case GPP:
			if((status->port_num < 0) || (status->port_num) > 14)
			{
				printk("GPP Port Num Range 0~14");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPP(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPP(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPP(status->port_num), status->value);
			break;

		case GPQ:
			if((status->port_num < 0) || (status->port_num) > 8)
			{
				printk("GPQ Port Num Range 0~8");
				return 0; 
			}
//			s3c_gpio_cfgpin(S3C64XX_GPQ(status->port_num), status->direction);
//			s3c_gpio_setpull(S3C64XX_GPQ(status->port_num), status->pull_updown);
			gpio_set_value(S3C64XX_GPQ(status->port_num), status->value);
			break;

		case WM8731:
			if(status->value)
			{
				//codec power on
				wm8731_poweron();
			}
			else
			{
				// codec power off
				wm8731_poweroff();
			}
			break;

		break;

		default:
		break;
	}
	return 1;
}

static int egpio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	egpio_status elisa_gpio_info;
	int size;
	size = _IOC_SIZE(cmd);
	
	switch(cmd)
	{
		case EGPIO_GET:
		mutex_lock(&egio_mutex);
		if(copy_from_user((void *) &elisa_gpio_info, (const void *) arg, size))
		{
			mutex_unlock(&egio_mutex);
			return -EFAULT;
		}
		mutex_unlock(&egio_mutex);

		read_port_status(&elisa_gpio_info);

		mutex_lock(&egio_mutex);
		if(copy_to_user((void *) arg, (const void *) &elisa_gpio_info, (unsigned long) size))
		{
			mutex_unlock(&egio_mutex);
			return -EFAULT;
		}
		mutex_unlock(&egio_mutex);
		break;

		case EGPIO_SET:
		mutex_lock(&egio_mutex);
		if(copy_from_user((void *) &elisa_gpio_info, (const void *) arg, size))
		{
			mutex_unlock(&egio_mutex);
			return -EFAULT;
		}
		mutex_unlock(&egio_mutex);

		write_port_status(&elisa_gpio_info);
		break;

		case EGPIO_STATUS:
		/* NOTHING */
		break;

		default:
		elisa_gpio_info.direction = GPIO_INPUT;
		elisa_gpio_info.port_group = GPNOTHING;
		elisa_gpio_info.port_num = 0;
		elisa_gpio_info.pull_updown = GPIO_PULL_NONE;
		elisa_gpio_info.value = 0;

		break;
	}
	return 0;
}

static int __init egpio_init(void)
{
	int ret;
	//printk("ELISA GPIO Init \n");
	ret = register_chrdev(GPIODEV_MAJOR, GPIODEV_NAME, &egpio_fops);

	if (ret < 0)
	{
		printk("Error registering ELISA GPIO device\n");
		return ret;
	}

	// janged USB VBUS OFF
       gpio_set_value(S3C64XX_GPM(2), 0);

	printk("Success registering ELISA GPIO device\n");
	return 0;
}

static void __exit egpio_exit(void)
{
	printk("ELISA GPIO Driver Exit\n");
}

struct file_operations egpio_fops =
{
	.owner		= THIS_MODULE,
	.read		= egpio_read,
	.write		= egpio_write,
	.ioctl			= egpio_ioctl,
	.open		= egpio_open,
	.release		= egpio_release,
};

module_init(egpio_init);
module_exit(egpio_exit);

MODULE_DESCRIPTION("ELISA GPIO Control Driver");
MODULE_AUTHOR("janged");
