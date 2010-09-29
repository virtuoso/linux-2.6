//-----------------------------------------------------------------------------
//      
// linux/drivers/video/epson/s1d13521fb.h -- 
// Function header for Epson S1D13521 controller frame buffer drivers.
//
// Copyright(c) Seiko Epson Corporation 2000-2008.
// All rights reserved.
//
// This file is subject to the terms and conditions of the GNU General Public
// License. See the file COPYING in the main directory of this archive for
// more details.
//
//---------------------------------------------------------------------------- 

#ifndef __S1D13521FB_H__
#define __S1D13521FB_H__

#include <linux/kernel.h>
#include <linux/fb.h>

#include "s1d13521ioctl.h"
#include S1D13xxxFB_INCLUDE

#include "../../elice/elice_hw_board_version.h"

#ifndef FALSE
  #define FALSE 0
#endif

#ifndef TRUE
  #define TRUE  (!FALSE)
#endif

#define HRDY_TIMEOUT_MS 3000//5000

// In Indirect Mode, a copy of the framebuffer is kept in system memory.
// A timer periodically writes this copy to the "real" framebuffer in
// hardware. This copy is called a virtual framebuffer.

//----------------------------------------------------------------------------
// Global structures used by s1d13521fb frame buffer code
//----------------------------------------------------------------------------
typedef struct
{
//        volatile unsigned char *RegAddr; 
//        unsigned RegAddrMappedSize;
        u32 VirtualFramebufferAddr;
        int blank_mode;
        u32 pseudo_palette[16];
}FB_INFO_S1D13521;

extern struct fb_fix_screeninfo s1d13521fb_fix;
extern struct fb_info s1d13521_fb;
extern FB_INFO_S1D13521 s1d13521fb_info;
extern char *s1d13521fb_version;

//-----------------------------------------------------------------------------
// Global Function Prototypes
//-----------------------------------------------------------------------------

#ifdef CONFIG_FB_EPSON_DEBUG_PRINTK
//#define dbg_info(fmt, args...) do { printk(KERN_INFO fmt, ## args); } while (0)
#define dbg_info(fmt, args...) do { } while (0)
#else 
#define dbg_info(fmt, args...) do { } while (0)
#endif

#ifdef CONFIG_FB_EPSON_DEBUG_PRINTK
#define assert(expr) \
        if(!(expr)) { \
        printk( "Assertion failed! %s,%s,%s,line=%d\n",\
        #expr,__FILE__,__FUNCTION__,__LINE__); \
        BUG(); \
        }
#else
#define assert(expr)
#endif


#ifdef CONFIG_FB_EPSON_PROC
int  __devinit s1d13521proc_init(void);
void __devexit s1d13521proc_terminate(void);
#endif

#ifdef CONFIG_FB_EPSON_PCI
int  __devinit s1d13521pci_init(long *physicalAddress);
void __devexit s1d13521pci_terminate(void);
#endif

int  __devinit s1d13521if_InterfaceInit(FB_INFO_S1D13521 *info);
void __devexit s1d13521if_InterfaceTerminate(FB_INFO_S1D13521 *info);
int  s1d13521if_cmd(unsigned ioctlcmd,s1d13521_ioctl_cmd_params *params,int numparams);
int s1d13521if_reg_cmd(unsigned ioctlcmd,s1d13521_ioctl_cmd_params *params,int numparams);
void s1d13521if_BurstWrite16_Area(u8 *ptr8, int x, int y, int width, int height);
void s1d13521if_BurstWrite16(u16 *ptr16, unsigned copysize16);
void s1d13521if_BurstRead16(u16 *ptr16, unsigned copysize16);
u16  s1d13521if_ReadReg16(u16 Index);
void s1d13521if_WriteReg16(u16 Index, u16 Value);
int  s1d13521if_WaitForHRDY(void);
void s1d13521fb_do_refresh_display(unsigned cmd,unsigned mode);
void s1d13521fb_init_display(void);
int s1d13521fb_InitRegisters(void);

int s1d13521fb_logo_display(int bProgress, int update_type);

//void s1d13521fb_white_display(void);
void s1d13521fb_gray_display(unsigned char GrayVal);

void bs_sfm_wait_for_bit( int ra, int pos, int val );//billy add
void s1d13521if_Reset(void);
void bs_init_clk(void);
void bs_init_spi(void) ;
void epson_sfm_bulk_erase(void);
void epson_sfm_write_data( int addr, int size, char * data );
void epson_ioctl_sfm_write_init( int addr);
void epson_ioctl_sfm_write_end(void);
void bs_sfm_wr_byte( int data );

#endif  //__S1D13521FB_H__
