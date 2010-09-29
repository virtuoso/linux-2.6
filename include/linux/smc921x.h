/***************************************************************************
 *
 * Copyright (C) 2004-2008 SMSC
 * Copyright (C) 2005-2008 ARM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************/
#ifndef __LINUX_SMSC921X_H__
#define __LINUX_SMSC921X_H__

/* platform_device configuration data, should be assigned to
 * the platform_device's dev.platform_data 
 * add all janged 
 */
struct smsc921x_platform_config {
	unsigned int irq_polarity;
	unsigned int irq_type;
	void (*hw_reset)(void);
};

#endif /* __LINUX_SMSC921X_H__ */
