#ifndef __ELICEDCC_H_
#define __ELICEDCC_H_

#define DVFS_IOCTL_MAGIC	'd'

typedef struct {
	unsigned int curr_play_state; //Low, High
	unsigned int curr_mode; //Low, High
	unsigned int music_rate; 
	unsigned int music_file_type;	
	unsigned long device_id;	
} elisa_dcc_info;

#define DCC_MUSIC_PLAYBACK_ON		_IOW(DVFS_IOCTL_MAGIC, 0, elisa_dcc_info)
#define DCC_MUSIC_PLAYBACK_OFF		_IO(DVFS_IOCTL_MAGIC, 1)
#define DCC_GET_STATUS				_IOR(DVFS_IOCTL_MAGIC, 2, elisa_dcc_info)
#define DCC_SET_STATUS				_IOW(DVFS_IOCTL_MAGIC, 3, elisa_dcc_info)
#define DCC_UPDATE_FINISH			_IO(DVFS_IOCTL_MAGIC, 4)
#define DCC_FIRMWARE_UPGRADE		_IO(DVFS_IOCTL_MAGIC, 5)
#define DCC_WAIT_CHANGE_LOW_CLOCK	_IO(DVFS_IOCTL_MAGIC, 6)
#define DCC_DO_CHANGE_LOW_CLOCK		_IO(DVFS_IOCTL_MAGIC, 7)

#include <mach/map.h>
#include <plat/regs-clock.h>
#include <plat/pll.h>

#define ARM_PLL_CON 	    S3C_APLL_CON
#define ARM_MPLL_CON 	    S3C_MPLL_CON
#define ARM_OTHERS 	    S3C_OTHERS
#define ARM_CLK_DIV	     S3C_CLK_DIV0
#define ARM_DIV_RATIO_BIT	0
#define ARM_DIV_MASK    (0xf<<ARM_DIV_RATIO_BIT)
#define HCLK_DIV_RATIO_BIT	9
#define HCLK_DIV_MASK  (0x7<<HCLK_DIV_RATIO_BIT)

#define MPLL_DIV_RATIO_BIT	4
//#define ARM_DIV_MASK    (0xf<<MPLL_DIV_RATIO_BIT)

#define SECURE_DIV_RATIO_BIT	18
#define HCLK_DIV_MASK  (0x7<<HCLK_DIV_RATIO_BIT)


#define READ_ARM_DIV    ((__raw_readl(ARM_CLK_DIV)&ARM_DIV_MASK) + 1)
#define PLL_CALC_VAL(MDIV,PDIV,SDIV)	((1<<31)|(MDIV)<<16 |(PDIV)<<8 |(SDIV)<<0)
#define GET_ARM_CLOCK(baseclk)	s3c6400_get_pll(__raw_readl(S3C_APLL_CON),baseclk)


#endif //__S3CDVS_H_
