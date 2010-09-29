/* linux/drivers/input/keyboard/s3c-keypad.h 
 *
 * Driver header for Samsung SoC keypad.
 *
 * Kim Kyoungil, Copyright (c) 2006-2009 Samsung Electronics
 *      http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _S3C_KEYPAD_H_
#define _S3C_KEYPAD_H_

static void __iomem *key_base;

#define KEYPAD_COLUMNS	8
#define KEYPAD_ROWS	8
#define MAX_KEYPAD_NR	64	/* 8*8 */
#define MAX_KEYMASK_NR	32

#if 1//ebook 10x5
// free num: 7, 15, 22, 23, 30, 31, 38, 39, 46, 47, 55, 63
int keypad_keycode[] = {
		// 0,			1,			2,			3,			4,			5,				6,			7,				8,			9
/*0*/	10, 				20, 			30, 			40, 			48, 			36, 				51, 			53/*7:power*/,	9, 			19, 
/*10*/	29, 				39, 			47, 			26, 			52, 			54/*15:hold*/,	8, 			18,				28, 			38, 
/*20*/	46, 				16, 			55,/*22:long power*/0, 			7, 			17, 				27, 			37,				45, 			6, 
/*30*/	0, 				0, 			4, 			14, 			24, 			34, 				44, 			5,				0, 			0, 
/*40*/	3, 				13, 			23, 			33, 			43, 			15, 				0, 			0,				2, 			12, 
/*50*/	22, 				32, 			42, 			25, 			50, 			0, 				1, 			11,				21, 			31,
/*60*/	41, 				35, 			49,			0
	};
#else
int keypad_keycode[] = {
		// 0,			1,			2,			3,			4,			5,			6,			7,			8,			9
/*0*/	0, 				1, 			2, 			3, 			4, 			5, 			6, 			7,			8, 			9, 
/*10*/	10, 				11, 			12, 			13, 			14, 			15, 			16, 			17,			18, 			19, 
/*20*/	20, 				21, 			22, 			23, 			24, 			25, 			26, 			27,			28, 			29, 
/*30*/	30, 				31, 			32, 			33, 			34, 			35, 			36, 			37,			38, 			39, 
/*40*/	40, 				41, 			42, 			43, 			44, 			45, 			46, 			47,			48, 			49, 
/*50*/	50, 				51, 			52, 			53, 			54, 			55, 			56, 			57,			58, 			59,
/*60*/	60, 				61, 			62,			63
	};
#endif

#ifdef CONFIG_CPU_S3C6410
#define KEYPAD_DELAY		(50)
#elif CONFIG_CPU_S5PC100
#define KEYPAD_DELAY		(600)
#endif

#define	KEYIFCOL_CLEAR		(readl(key_base+S3C_KEYIFCOL) & ~0xffff)
#define	KEYIFCON_CLEAR		(readl(key_base+S3C_KEYIFCON) & ~0x1f)
#define KEYIFFC_DIV		(readl(key_base+S3C_KEYIFFC) | 0x1)

struct s3c_keypad {
	struct input_dev *dev;
	int nr_rows;	
	int no_cols;
	int total_keys; 
	int keycodes[MAX_KEYPAD_NR];
};

extern void s3c_setup_keypad_cfg_gpio(int rows, int columns);

#endif				/* _S3C_KEYIF_H_ */
