/*
 * Control saving and restoring of coresight components' state during
 * OFF mode.
 *
 * Copyright (C) 2010 Nokia Corporation
 * Alexander Shishkin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include "pm.h"

/*
 * Pointer to a place in sram where the ETM/debug state save
 * flag is. It can be calculated after the omap_sram_idle is
 * pushed to sram.
 */
static unsigned int *_etm_save;

/*
 * sysfs file /sys/power/coresight_save controls whether the
 * state of coresight components should be saved and restored
 * across OFF modes.
 */
static ssize_t coresight_save_show(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%u\n", *_etm_save);
}

static ssize_t coresight_save_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t n)
{
	unsigned int value;

	if (sscanf(buf, "%u", &value) != 1)
		return -EINVAL;

	*_etm_save = !!value;

	return n;
}

static struct kobj_attribute coresight_save_attr =
	__ATTR(coresight_save, 0644, coresight_save_show, coresight_save_store);

int omap3_coresight_pm_init(void *sram_addr)
{
	int ret;

	/* the last word from the top of omap_sram_idle */
	_etm_save = (unsigned *)((u8 *)sram_addr + omap34xx_cpu_suspend_sz - 4);

	ret = sysfs_create_file(power_kobj, &coresight_save_attr.attr);

	return ret;
}

