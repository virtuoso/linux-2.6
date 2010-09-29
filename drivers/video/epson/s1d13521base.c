//-----------------------------------------------------------------------------
//      
// linux/drivers/video/epson/s1d13521fb.c -- frame buffer driver for Epson
// S1D13521 series of LCD controllers.
// 
// Copyright(c) Seiko Epson Corporation 2000-2008.
// All rights reserved.
//
// This file is subject to the terms and conditions of the GNU General Public
// License. See the file COPYING in the main directory of this archive for
// more details.
//
//---------------------------------------------------------------------------- 

//
// factory 커널.
//
#define EPD_ENABLE_SLEEP
#ifdef CONFIG_SUPPORT_FACTORY
#undef EPD_ENABLE_SLEEP
#endif//CONFIG_SUPPORT_FACTORY

#define S1D13521FB_VERSION              "S1D13521FB: $Revision: 0 $"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>

#ifdef CONFIG_FB_EPSON_PCI
    #include <linux/pci.h>
#endif

#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include "s1d13521fb.h"
#include <linux/timer.h>

#include <linux/irq.h>
#include <mach/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>


#ifdef CONFIG_FB_EPSON_GPIO_S3C6410
    #ifdef CONFIG_FB_EPSON_PCI 
    #undef CONFIG_FP_EPSON_PCI
    #endif
#endif

#ifdef VIDEO_REFRESH_PERIOD  
#undef VIDEO_REFRESH_PERIOD
#endif


#define EPD_DEBUG
#ifdef EPD_DEBUG
#define epddbg(X...) do { printk("<kernel : dcc> %s(): ", __func__); printk(X); } while(0)
#else
#define epddbg(format, arg...) do {} while (0)
#endif
// This is the refresh period for updating the display for RAM based LCDs 
// and/or indirect interfaces.  It is set to (1 second / X).  This value can
// modified by the end-user.  Be aware that decreasing the refresh period
// increases CPU usage as well.
// There are HZ (typically 100) jiffies per second
#if CONFIG_FB_EPSON_VIRTUAL_FRAMEBUFFER_FREQ != 0

//woong
//#define VIDEO_REFRESH_PERIOD   (HZ/CONFIG_FB_EPSON_VIRTUAL_FRAMEBUFFER_FREQ)
#define VIDEO_REFRESH_PERIOD   (HZ/20)
#endif

// This section only reports the configuration settings
#ifdef CONFIG_FB_EPSON_SHOW_SETTINGS
        #warning ########################################################################
        #warning Epson S1D13521 Frame Buffer Configuration:

        #ifdef CONFIG_FB_EPSON_PCI
                #warning Using PCI interface
        #else
                #warning Not using PCI interface
        #endif

        #ifdef CONFIG_FB_EPSON_GPIO_S3C6410
                #warning s3c6410 port using GPIO pins
        #else
                #warning Not s3c6410 port
        #endif

        #ifdef CONFIG_FB_EPSON_PROC
                #warning Adding /proc functions
        #else
                #warning No /proc functions
        #endif

        #ifdef CONFIG_FB_EPSON_DEBUG_PRINTK
                #warning Enable debugging printk() calls
        #else
                #warning No debugging printk() calls
        #endif

        #ifdef CONFIG_FB_EPSON_BLACK_AND_WHITE
                #warning Virtual Framebuffer Black And White
        #else
                #warning Virtual Framebuffer 16 Shades of Gray
        #endif
        
        #ifdef VIDEO_REFRESH_PERIOD
                #warning Timer video refresh ENABLED.
        #else
                #warning Timer video refresh DISABLED.
        #endif

        #ifdef CONFIG_FB_EPSON_HRDY_OK
                #warning Assuming HRDY signal present
        #else
                #warning Assuming HRDY signal NOT present. 
        #endif
        #warning ########################################################################
#endif

//woong
//#define SUPPORT_16_GRAY 1

//-----------------------------------------------------------------------------
//
// Local Definitions     
// 
//---------------------------------------------------------------------------

#define S1D_MMAP_PHYSICAL_REG_SIZE      sizeof(S1D13XXXFB_IND_STRUCT)
#define VFB_SIZE_BYTES ((S1D_DISPLAY_WIDTH*S1D_DISPLAY_HEIGHT*S1D_DISPLAY_BPP)/8)

//-----------------------------------------------------------------------------
//
// Function Prototypes
//
//-----------------------------------------------------------------------------
int  __devinit s1d13521fb_init(void);
int  __devinit s1d13521fb_setup(char *options, int *ints);
void __exit s1d13521fb_exit(void);

static int  s1d13521fb_set_par(struct fb_info *info);
static void s1d13521fb_fillrect(struct fb_info *p, const struct fb_fillrect *rect);
static void s1d13521fb_copyarea(struct fb_info *p, const struct fb_copyarea *area);
static void s1d13521fb_imageblit(struct fb_info *p, const struct fb_image *image);
static int  s1d13521fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int  s1d13521fb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info);
static int  s1d13521fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);
//static int  s1d13521fb_blank(int blank_mode, struct fb_info *info);
static int  s1d13521fb_mmap(struct fb_info *info, struct vm_area_struct *vma);
static int  s1d13521fb_set_virtual_framebuffer(void);
#ifdef VIDEO_REFRESH_PERIOD
static void s1d13521fb_refresh_display(unsigned long dummy);
#endif
extern int get_elice_board_version(void);

//-----------------------------------------------------------------------------
//
// Globals
//
//-----------------------------------------------------------------------------

char *s1d13521fb_version  = S1D13521FB_VERSION;
//static int gDisplayChange = 0;          // Display update needed flag

//static char cur_epd_rotation_mode = EPD_ROTATION_90;
static char cur_epd_rotation_mode = EPD_ROTATION_270;
//static char cur_epd_power_mode = EPD_POWER_NORMAL;//normal, sleep두가지만 사용함.
static char retry_count = 0;

static struct fb_ops s1d13521fb_ops =
{
        .owner          = THIS_MODULE,
        .fb_set_par     = s1d13521fb_set_par,
        .fb_check_var   = s1d13521fb_check_var,
        .fb_setcolreg   = s1d13521fb_setcolreg,
        .fb_fillrect    = s1d13521fb_fillrect,
        .fb_copyarea    = s1d13521fb_copyarea,
        .fb_imageblit   = s1d13521fb_imageblit,
//      .fb_cursor      = soft_cursor,
        .fb_mmap        = s1d13521fb_mmap,
        .fb_ioctl       = s1d13521fb_ioctl,
        .fb_blank 	= NULL,//billy add
        .fb_pan_display = NULL,//billy add
        .fb_cursor 	= NULL,//billy add
};

struct fb_fix_screeninfo s1d13521fb_fix = 
{
        .id             = "s1d13521", 
        .type           = FB_TYPE_PACKED_PIXELS,
        .type_aux       = 0,
        .xpanstep       = 0,
        .ypanstep       = 0,
        .ywrapstep      = 0,
        .smem_len       = S1D_DISPLAY_WIDTH*S1D_DISPLAY_HEIGHT*S1D_DISPLAY_BPP,
        .line_length    = S1D_DISPLAY_WIDTH*S1D_DISPLAY_BPP/8, 
        .accel          = FB_ACCEL_NONE,
};

struct fb_info   s1d13521_fb;
FB_INFO_S1D13521 s1d13521fb_info;

#ifdef VIDEO_REFRESH_PERIOD
struct timer_list s1d13521_timer;
#endif

#define BOOTING_REFRESH_PERIOD	(HZ/4)
#ifdef BOOTING_REFRESH_PERIOD
struct timer_list s1d13521_booting_timer;
#endif//BOOTING_REFRESH_PERIOD


int is_s1d13521fb_normal_mode(void)
{
	int b=0;
	int v = s1d13521if_ReadReg16( 0x00a );
	//dbg_info("%s(), reg(0x00a): 0x%x\n",__FUNCTION__, v);

	b = ( v >> 10 ) & 0x3;

	if( b == 0x1) //0x01
	{
		//dbg_info("%s(), normal mode, b: 0x%x\n",__FUNCTION__, b);
		return 1;
	}

	return 0;
}


int is_s1d13521fb_sleep_mode(void)
{
	int b=0;
	int v = s1d13521if_ReadReg16( 0x00a );
	//dbg_info("%s(), reg(0x00a): 0x%x\n",__FUNCTION__, v);

	b = ( v >> 10 ) & 0x3;

	if( b == 0x3) //0x3
	{
		//dbg_info("%s(), sleep mode, b: 0x%x\n",__FUNCTION__, b);
		return 1;
	}

	return 0;
}

/* Added by woong */
static unsigned char curEPDMode = 0;
void set_cur_epd_mode(unsigned char mode)
{
	curEPDMode = mode;
}

unsigned char get_cur_epd_mode(void)
{
	return curEPDMode;
}

EXPORT_SYMBOL(get_cur_epd_mode);

void s1d13521fb_goto_normal(void)
{
	//power mode확인.
	s1d13521if_WaitForHRDY();
	
	if( is_s1d13521fb_normal_mode() )
	{
		return;
	}
	else
	{
		unsigned cmd,numparams;
		s1d13521_ioctl_cmd_params cmd_params;

		s1d13521if_WaitForHRDY();
		s1d13521if_WriteReg16(0x006,0x000);//run pll, clki

		mdelay(2); // 4ms

		cmd = RUN_SYS;//run. for normal mode.
		numparams = 0;
		cmd_params.param[0] = 0;
		cmd_params.param[1] = 0;
		cmd_params.param[2] = 0;
		cmd_params.param[3] = 0;
		cmd_params.param[4] = 0;
		if(s1d13521if_reg_cmd(cmd,&cmd_params,numparams)<0)
		{
			dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
			//return -1;
		}
#if 0
		mdelay(22); 
#else
		s1d13521if_WaitForHRDY();
		mdelay(20); 
#endif
		
		set_cur_epd_mode(0);
		//dbg_info("==> %s(), normal mode ok!!\n",__FUNCTION__); 
	}
}

extern int get_cur_clock_state(void);

void s1d13521fb_goto_sleep(void)
{
	s1d13521if_WaitForHRDY();
	
	if( is_s1d13521fb_sleep_mode() )
	{
		return;
	}
	else
	{
		unsigned cmd,numparams;
		s1d13521_ioctl_cmd_params cmd_params;

		s1d13521if_WaitForHRDY();
			
		//epddbg("[k : %s] before EPD sleep mode !! \n",__FUNCTION__); 
		cmd = SLP;//sleep.
		numparams = 0;
		cmd_params.param[0] = 0;
		cmd_params.param[1] = 0;
		cmd_params.param[2] = 0;
		cmd_params.param[3] = 0;
		cmd_params.param[4] = 0;
		if(s1d13521if_reg_cmd(cmd,&cmd_params,numparams)<0)
		{
			dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
			//return -1;
		}
#if 0
		mdelay(22); 
#else
		s1d13521if_WaitForHRDY();

		if (get_cur_clock_state() == 2)  //low clock : 즉 음악 듣고 있느중...
			udelay(20);
		else
			mdelay(20); 
#endif
		//dbg_info("==> %s(), epd sleep mode ok!!\n",__FUNCTION__); 
		//epddbg("[k : %s] EPD sleep mode ok!!\n",__FUNCTION__);
		set_cur_epd_mode(1);
	}
}


void ui_draw_fill_box(char gray, int x, int y, int width, int heiht)
{
	int i, j;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	char* p_buffer8_area = p_buffer8+ (y*S1D_DISPLAY_WIDTH + x);
	
	for (j = 0; j < heiht; j++)
	{
		for(i = 0; i < width; i++)
		{
			*(p_buffer8_area + j*S1D_DISPLAY_WIDTH + i) =  gray;
		}
	}
}


#define LEVEL_3			3
#define LEVEL_2			2
#define LEVEL_1			1
#define LEVEL_BLANK 		0
#define LEVEL_LOW		99
extern unsigned int get_cur_battery_level(void);
void ui_draw_battery(void)
{
	int x, y, w, h;
	int battery_level = get_cur_battery_level();

	x = 10;
	y = 10;
	w = 5;
	h = 20;
//janged
//poewr off 되는 동아 printk 없도록 하려고 아래 printk 전부 막음 

	if(battery_level == LEVEL_3)
	{
		//printk("[kernel, %s] LEVEL_3\n", __FUNCTION__);
		ui_draw_fill_box(0x40, x, y, w, h);
		ui_draw_fill_box(0x40, x+w+3, y, w, h);
		ui_draw_fill_box(0x40, x+(w+3)*2, y, w, h);
	}
	else if(battery_level == LEVEL_2)
	{
		//printk("[kernel, %s] LEVEL_2]n", __FUNCTION__);
		ui_draw_fill_box(0x40, x, y, w, h);
		ui_draw_fill_box(0x40, x+w+3, y, w, h);
		//ui_draw_fill_box(0x40, x+(w+3)*2, y, w, h);
	}
	else if(battery_level == LEVEL_1)
	{
		//printk("[kernel, %s] LEVEL_1\n", __FUNCTION__);
 		ui_draw_fill_box(0x40, x, y, w, h);
		//ui_draw_fill_box(0x40, x+w+3, y, w, h);
		//ui_draw_fill_box(0x40, x+(w+3)*2, y, w, h);
	}
	else if(battery_level == LEVEL_BLANK)
	{
		//printk("[kernel, %s] LEVEL_BLANK\n", __FUNCTION__);
		ui_draw_fill_box(0x40, x, y, w*3+6, h);
		ui_draw_fill_box(0xff, x+1, y+1, w*3+6-2, h-2);
	}
	else//LEVEL_LOW
	{
		//printk("[kernel, %s] LEVEL_LOW\n", __FUNCTION__);
		ui_draw_fill_box(0x40, x, y, w*3+6, h);
		ui_draw_fill_box(0xff, x+1, y+1, w*3+6-2, h-2);
		ui_draw_fill_box(0xff, x+3, y, w*3, h);
	}
}


extern char logo_iriver_img[];
void ui_draw_logo(void)
{
	int i, j;
	int img_x, img_y;
	int img_width, img_height;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

	//dbg_info("%s()++\n",__FUNCTION__);

	img_x = 167;
	img_y = 494;

	img_width = 115;
	img_height = 54;
	
	for (j = 0; j < S1D_DISPLAY_HEIGHT; j++)
	{
		for(i = 0; i < S1D_DISPLAY_WIDTH; i++)
		{
			if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) =  logo_iriver_img[(j-img_y)*img_width + (i-img_x)] ;
			}
			else
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xff;
			}
		}
	}

	//
	// draw battery ui!!
	//
	//ui_draw_battery();

	//dbg_info("%s()--\n",__FUNCTION__);
}


extern char battery_no_img[];
void ui_draw_battery_no(void)
{
	int i, j;
	int img_x, img_y;
	int img_width, img_height;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

	//dbg_info("%s()++\n",__FUNCTION__);

	img_x = 214;
	img_y = 301;

	img_width = 172;
	img_height = 142;
	
	for (j = 0; j < S1D_DISPLAY_HEIGHT; j++)
	{
		for(i = 0; i < S1D_DISPLAY_WIDTH; i++)
		{
			if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) =  battery_no_img[(j-img_y)*img_width + (i-img_x)] ;
			}
			else
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xff;
			}
		}
	}
	//dbg_info("%s()--\n",__FUNCTION__);
}


///////////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// Parse user specified options (`video=s1d13521:')
// Example: 
// video=s1d13521:noaccel
//-----------------------------------------------------------------------------
int __init s1d13521fb_setup(char *options, int *ints)
{
        return 0;
}

//----------------------------------------------------------------------------- 
// 
// Fill in the 'var' and 'fix' structure. 
//      
//----------------------------------------------------------------------------- 
static int s1d13521fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
        var->xres               = S1D_DISPLAY_WIDTH;     
        var->yres               = S1D_DISPLAY_HEIGHT;
        var->xres_virtual       = var->xres;    
        var->yres_virtual       = var->yres;
        var->xoffset            = var->yoffset = 0;
        var->bits_per_pixel     = S1D_DISPLAY_BPP;
        //var->grayscale          = 1;
        var->grayscale          = 8;//billy
        var->nonstd             = 0;                    /* != 0 Non standard pixel format */
        var->activate           = FB_ACTIVATE_NOW;      /* see FB_ACTIVATE_*             */
        var->height             = -1;                   /* height of picture in mm       */
        var->width              = -1;                   /* width of picture in mm        */
        var->accel_flags        = 0;                    /* acceleration flags (hints     */
        var->pixclock           = S1D_DISPLAY_PCLK;
        var->right_margin       = 0;
        var->lower_margin       = 0;
        var->hsync_len          = 0;
        var->vsync_len          = 0;
        var->left_margin        = 0;
        var->upper_margin       = 0;
        var->sync               = 0;
        var->vmode              = FB_VMODE_NONINTERLACED;
        var->red.msb_right      = var->green.msb_right = var->blue.msb_right = 0;
        var->transp.offset      = var->transp.length = var->transp.msb_right = 0;

        s1d13521fb_fix.visual = FB_VISUAL_TRUECOLOR;
  
        switch (info->var.bits_per_pixel)
        {
                case 1:
                case 2:
                case 4:
                case 5:
                case 8:
                        var->red.offset  = var->green.offset = var->blue.offset = 0;
                        var->red.length  = var->green.length = var->blue.length = S1D_DISPLAY_BPP;
                        break;  

                default:
                        printk(KERN_WARNING "%dbpp is not supported.\n",
                                        info->var.bits_per_pixel);
                        return -EINVAL;
        }
 
        return 0;
}  


//---------------------------------------------------------------------------- 
// Set a single color register. The values supplied have a 16 bit  
// magnitude.  
// Return != 0 for invalid regno.  
//
// We get called even if we specified that we don't have a programmable palette
// or in direct/true color modes!
//---------------------------------------------------------------------------- 
static int s1d13521fb_setcolreg(unsigned regno, unsigned red, unsigned green,
                                unsigned blue, unsigned transp, struct fb_info *info) 
{
        // Make the first 16 LUT entries available to the console
        if (info->var.bits_per_pixel == 8 && s1d13521fb_fix.visual == FB_VISUAL_TRUECOLOR)
        {
                if (regno < 16)
                {       
                        // G= 30%R + 59%G + 11%B
                        unsigned gray = (red*30 + green*59 + blue*11)/100;
                        gray = (gray>>8) & 0xFF;

#ifdef CONFIG_FB_EPSON_BLACK_AND_WHITE
                        if (gray != 0)
                                gray = 0xFF;
#endif
                        
                        // invert: black on white
                        gray = 0xFF-gray;               
                        ((u32*)info->pseudo_palette)[regno] = gray;

/*
                        dbg_info("%s(): regno=%02Xh red=%04Xh green=%04Xh blue=%04Xh transp=%04Xh ->gray=%02Xh\n",
                                __FUNCTION__, regno, red, green, blue, transp,gray);
*/
                }
        }
        else
                return 1;

        return 0; 
}

//----------------------------------------------------------------------------- 
//
// Set the hardware. 
//
//----------------------------------------------------------------------------- 
static int s1d13521fb_set_par(struct fb_info *info)
{
        info->fix = s1d13521fb_fix;
// 나중에 확인 billy.
//        info->fix.mmio_start = (unsigned long)virt_to_phys((void*)s1d13521fb_info.RegAddr);
//        info->fix.mmio_len   = s1d13521fb_info.RegAddrMappedSize; //S1D_MMAP_PHYSICAL_REG_SIZE;
        info->fix.smem_start = virt_to_phys((void*)s1d13521fb_info.VirtualFramebufferAddr);
        info->screen_base    = (unsigned char*)s1d13521fb_info.VirtualFramebufferAddr;
        info->screen_size    = (S1D_DISPLAY_WIDTH*S1D_DISPLAY_HEIGHT*S1D_DISPLAY_BPP)/8;

        return 0;
}

//----------------------------------------------------------------------------
//
// PRIVATE FUNCTION:
// Remaps virtual framebuffer from virtual memory space to a physical space.
//
//      If no virtual framebuffer is used, the default fb_mmap routine should
//      be fine, unless there is a concern over its use of 
//              io_remap_pfn_range versus remap_pfn_range
//
//----------------------------------------------------------------------------
static int s1d13521fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
        unsigned long off;
        unsigned long start;
        u32 len;

	dbg_info("%s()\n",__FUNCTION__); 

        off = vma->vm_pgoff << PAGE_SHIFT;

        // frame buffer memory 
        start = info->fix.smem_start;
        len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);

        if (off >= len) 
        {
                // memory mapped io 
                off -= len;

                if (info->var.accel_flags) 
                        return -EINVAL;
                
                start = info->fix.mmio_start;
                len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.mmio_len);
        }

        start &= PAGE_MASK;

        if ((vma->vm_end - vma->vm_start + off) > len)
                return -EINVAL;

        off += start;
        vma->vm_pgoff = off >> PAGE_SHIFT;

        // This is an IO map - tell maydump to skip this VMA 
        vma->vm_flags |= VM_RESERVED;

        if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
                             vma->vm_end - vma->vm_start, vma->vm_page_prot))
                return -EAGAIN;

        return 0;
}


/*
1~4 byte: cmd		-1-
5~8 byte: x position	-2-
9~12byte: y position	-3-
13~16byte: width		-4-
17~20byte: height		-5-

21~32byte; reserved.
*/
int s1d13521fb_get_virtual_framebuffer_cmd(void)
{
	char* p_buffer8 = (char*)s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	return *(p_cmd32 + 1);
}

void s1d13521fb_set_virtual_framebuffer_cmd(int inCmd)
{
	char* p_buffer8 = (char*)s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);
	
	*(p_cmd32 + 1) = inCmd;
}

int s1d13521fb_get_virtual_framebuffer_area_x(void)
{
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	return *(p_cmd32 + 2);
}

int s1d13521fb_get_virtual_framebuffer_area_y(void)
{
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	return *(p_cmd32 + 3);
}

int s1d13521fb_get_virtual_framebuffer_area_width(void)
{
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	return *(p_cmd32 + 4);
}

int s1d13521fb_get_virtual_framebuffer_area_height(void)
{
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	return *(p_cmd32 + 5);
}


void s1d13521fb_set_virtual_framebuffer_area(int x, int y, int width, int height)
{
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;
	int* p_cmd32 = (int *)(p_buffer8 + S1D_DISPLAY_HEIGHT*S1D_DISPLAY_WIDTH);

	*(p_cmd32 + 2) = x;
	*(p_cmd32 + 3) = y;
	*(p_cmd32 + 4) = width;
	*(p_cmd32 + 5) = height;
}


void s1d13521fb_cpoy_virtual_framebuffer_area(char *img_area, int img_x, int img_y, int img_width, int img_height)
{
	int i, j, pos;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

	pos = 0;
	for (j = img_y; j < img_height; j++)
	{
		for(i = img_x; i < img_width; i++)
		{
			*(img_area+pos) = *(p_buffer8 + j*S1D_DISPLAY_WIDTH + i);
			pos++;
		}
	}	
}

//----------------------------------------------------------------------------
// PRIVATE FUNCTION:
// Allocates virtual framebuffer.
//----------------------------------------------------------------------------
static int s1d13521fb_set_virtual_framebuffer(void)
{
        u32 order = 0;
        u32 size = VFB_SIZE_BYTES;
        u32 addr;

	//billy add, cmd추가 해보장..~~
	/***********************/
	size += 32;
	/***********************/

	//dbg_info("%s()\n",__FUNCTION__); 

        while (size > (PAGE_SIZE * (1 << order)))
                order++;

        //s1d13521fb_info.VirtualFramebufferAddr = __get_free_pages(GFP_KERNEL, order);
        s1d13521fb_info.VirtualFramebufferAddr = __get_dma_pages(GFP_KERNEL, order);//billy dma메모리로 할당.

        if (s1d13521fb_info.VirtualFramebufferAddr == 0)
        {
                printk(KERN_WARNING "%s(): Could not allocate memory for virtual display buffer.\n", __FUNCTION__);
                return 1;
        }

        for (addr = s1d13521fb_info.VirtualFramebufferAddr; addr < (s1d13521fb_info.VirtualFramebufferAddr+size); addr += PAGE_SIZE)
                SetPageReserved(virt_to_page(addr));

	//dbg_info("%s(): s1d13521fb_info.VirtualFramebufferAddr: 0x%x\n",__FUNCTION__, s1d13521fb_info.VirtualFramebufferAddr); 

        return 0;
}

//----------------------------------------------------------------------------
// s1d13521fb_do_refresh_display(void): unconditionally refresh display:
// This function updates the display
//----------------------------------------------------------------------------
//#define VFB_GC_COUNT    60
//static int gGCCount = VFB_GC_COUNT;

//#define VFB_IDLE_COUNT  120
//static int gIdleCount = 0;


void s1d13521fb_do_refresh_display(unsigned cmd,unsigned mode)
{
        // Copy virtual framebuffer to display framebuffer.
	s1d13521_ioctl_cmd_params cmd_params;
	u32     size16  = VFB_SIZE_BYTES/2;
	u16 *   pSource = (u16*) s1d13521fb_info.VirtualFramebufferAddr;

	unsigned reg330 = s1d13521if_ReadReg16(0x330);

	s1d13521if_WriteReg16(0x330,0x84);              // LUT AutoSelect + P4N
	
	// Copy virtual framebuffer to hardware via indirect interface burst mode write
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
		
#ifdef CONFIG_FB_EPSON_ENABLE_4BPP
	cmd_params.param[0] = (0x2<<4);// 1 byte per pixel.(billy check, 0x2<<4하면 4bpp)
#else
	cmd_params.param[0] = (0x3<<4);// 1 byte per pixel.(billy check, 0x2<<4하면 4bpp)
#endif//CONFIG_FB_EPSON_ENABLE_4BPP
	s1d13521if_cmd(LD_IMG,&cmd_params,1);

	cmd_params.param[0] = 0x154;
	s1d13521if_cmd(WR_REG,&cmd_params,1);
	s1d13521if_BurstWrite16(pSource,size16);
	s1d13521if_cmd(LD_IMG_END,&cmd_params,0);

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!
	
	cmd_params.param[0] = (mode<<8);
	//cmd_params.param[0] = (mode<<8) | (1<<14);//border...
	s1d13521if_cmd(cmd,&cmd_params,1);              // update all pixels
	
	//bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	//bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	s1d13521if_cmd(WAIT_DSPE_FREND,&cmd_params,0);
	s1d13521if_WriteReg16(0x330,reg330);            // restore the original reg330
}       



void s1d13521fb_do_area_refresh_display(unsigned cmd,unsigned mode, int x, int y, int width, int height)
{
        // Copy virtual framebuffer to display framebuffer.
	s1d13521_ioctl_cmd_params cmd_params;
	u8 *  pSource = (u8 *) s1d13521fb_info.VirtualFramebufferAddr;

	//dbg_info("%s() x: %d, y: %d, width: %d, height: %d\n",__FUNCTION__, x, y, width, height);

	unsigned reg330 = s1d13521if_ReadReg16(0x330);
	s1d13521if_WriteReg16(0x330,0x84);              // LUT AutoSelect + P4N

	// Copy virtual framebuffer to hardware via indirect interface burst mode write
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);

#if 1
#ifdef CONFIG_FB_EPSON_ENABLE_4BPP
	cmd_params.param[0] = (0x2<<4);// 4bit per pixel.(billy check, 0x2<<4하면 4bpp)
#else
	cmd_params.param[0] = (0x3<<4);// 1 byte per pixel.
#endif//CONFIG_FB_EPSON_ENABLE_4BPP
	cmd_params.param[1] = x;
	cmd_params.param[2] = y;
	cmd_params.param[3] = width;
	cmd_params.param[4] = height;
	s1d13521if_cmd(LD_IMG_AREA,&cmd_params,5);
#else
	cmd_params.param[0] = (0x3<<4);// 1 byte per pixel.
	s1d13521if_cmd(LD_IMG,&cmd_params,1);
#endif

	cmd_params.param[0] = 0x154;
	s1d13521if_cmd(WR_REG,&cmd_params,1);

	//s1d13521if_BurstWrite16(pSource, size16);
	s1d13521if_BurstWrite16_Area(pSource, x, y, width, height);

	s1d13521if_cmd(LD_IMG_END,&cmd_params,0);

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	cmd_params.param[0] = (mode<<8);
	cmd_params.param[1] = (unsigned short) x;
	cmd_params.param[2] = (unsigned short) y;
	cmd_params.param[3] = (unsigned short) width;
	cmd_params.param[4] = (unsigned short) height;
	s1d13521if_cmd(cmd,&cmd_params,5);              // update all pixels

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	s1d13521if_cmd(WAIT_DSPE_FREND,&cmd_params,0);

	s1d13521if_WriteReg16(0x330,reg330);            // restore the original reg330
}   


int s1d13521fb_check_rotation(int epd_iriver_cmd)
{
	if( !(epd_iriver_cmd == EPD_ROTATION_0
		|| epd_iriver_cmd == EPD_ROTATION_90
		|| epd_iriver_cmd == EPD_ROTATION_180
		|| epd_iriver_cmd == EPD_ROTATION_270
		))
	{
		return epd_iriver_cmd;
	}
	
	if(cur_epd_rotation_mode == epd_iriver_cmd)
	{
		//part update.
		return EPD_IRIVER_CMD_PART;
	}
	else
	{
		s1d13521_ioctl_cmd_params cmd_params;
		
		//full update.
		cur_epd_rotation_mode = epd_iriver_cmd;

		//operate rotation.
		switch (epd_iriver_cmd)
        	{
        		/*rotation mode => 0: 0 degrees, 1: 90 degrees, 2: 180 degrees, 3: 270 degrees*/
        		case EPD_ROTATION_0:
				cmd_params.param[0] = (0 << 8);
				break;

			case EPD_ROTATION_90:
				cmd_params.param[0] = (1 << 8);
				break;

			case EPD_ROTATION_180:
				cmd_params.param[0] = (2 << 8);
				break;

			case EPD_ROTATION_270:
				cmd_params.param[0] = (3 << 8);
				break;
				
			default:
				return epd_iriver_cmd;
		}		
		
		cmd_params.param[1] = 0;
		cmd_params.param[2] = 0;
		cmd_params.param[3] = 0;
		cmd_params.param[4] = 0;
		s1d13521if_cmd(INIT_ROTMODE, &cmd_params, 1);

		mdelay(5);

		return EPD_IRIVER_CMD_FULL;
	}
}


int wait_for_lut_free( void )
{
	int cnt = 1000;
	int v = s1d13521if_ReadReg16( 0x0336 );
	
	while ( true ) 
	{
		v = s1d13521if_ReadReg16( 0x0336 );
		if ( v != 0xFFFF ) break;
		dbg_info("%s() now waiting!!\n",__FUNCTION__);

		if(cnt<0)
		{
			return -1;
		}
		cnt--;

		udelay(100);
	} // while

	return 1;
}

int wait_for_trigger_busy(void)
{
	int cnt = 1000;
	int v = s1d13521if_ReadReg16( 0x338 );
	
	while ( ( ( v >> 0 ) & 0x1 ) != ( 0 & 0x1 ) )
	{
		v = s1d13521if_ReadReg16( 0x338 );
		dbg_info("%s() now waiting!!\n",__FUNCTION__);

		if(cnt<0)
		{
			return -1;
		}
		cnt--;

		udelay(100);
	}
	return 1;
}

int wait_for_display_frame_busy(void)
{
	int cnt = 1000;
	int v = s1d13521if_ReadReg16( 0x338 );

	//dbg_info("%s()++\n",__FUNCTION__);
	//dbg_info("[kernel]epd busy++\n");
	while ( ( ( v >> 3 ) & 0x1 ) != ( 0 & 0x1 ) ) 
	{
		v = s1d13521if_ReadReg16( 0x338 );
		//dbg_info("%s() now waiting!!\n",__FUNCTION__);

		if(cnt<0)
		{
			return -1;
		}
		cnt--;

		mdelay(1);
	}	

	return 1;
	//dbg_info("%s()--\n",__FUNCTION__);
	//dbg_info("[kernel]epd busy--\n");
}





static void s1d13521fb_hirq_isr_bh(void *data)
{
	printk("==========> [epd, %s] HIRQ!!\n", __FUNCTION__);
}
static DECLARE_WORK(s1d13521fb_hirq_isr_work, (void *)&s1d13521fb_hirq_isr_bh);

#if 0
//janged 형승아 아래 함수 사용하는 데가 없다. 
static irqreturn_t s1d13521fb_hirq_isr(int irq, void *dev_id)
{
	schedule_work(&s1d13521fb_hirq_isr_work);

	return IRQ_HANDLED;
}
#endif


//----------------------------------------------------------------------------
// PRIVATE FUNCTION:
// This function updates the display
//----------------------------------------------------------------------------
#ifdef VIDEO_REFRESH_PERIOD
long epd_pre_time = 0;
int epson_epd_reboot(int gray_val);
int error_need_epd_reboot = 0;//epd 이상해지면 epd만 reboot.
extern void read_temperature_value(void);
extern void force_high_clock(void);
extern void set_readyToChangeClock(int input);
extern unsigned char get_cur_sleep_mode(void);
extern unsigned char get_system_is_sleep_mode(void);

static void s1d13521fb_refresh_display(unsigned long dummy)
{
	int epd_iriver_cmd = s1d13521fb_get_virtual_framebuffer_cmd();
	unsigned mode, cmd =0x00;

	//dbg_info("%s()*** epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);
	//printk("%s()*** epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);

	if(epd_iriver_cmd == EPD_IRIVER_CMD_POWEROFF)
	{
		printk("%s(), Now EPD poweroff, Does this function work?\n",__FUNCTION__);
		return;
	}

	//woong : sleep 모드에 진입한 후로는 빠져나올때 까지 return해준다.
	if (get_system_is_sleep_mode() == 1)
	{
		if (epd_iriver_cmd != 0x0)
		printk("%s(), Now system is in the sleep mode, event 0x%x !!\n",__FUNCTION__, epd_iriver_cmd);
		return;
	}

	if(error_need_epd_reboot)
	{
		printk("%s(), now epd reboot!!!\n",__FUNCTION__);
		epson_epd_reboot(-1);
		error_need_epd_reboot = 0;
	}

	//
	// HIRQ
	//
	if(get_elice_board_version() == ELICE_BOARD_WS1)
	{
		//폴링으로 체트함. WS보드에서만 사용.
		if(gpio_get_value(S3C64XX_GPM(3)) >0)
		{
			s1d13521fb_hirq_isr_bh(NULL);
		}
	}

	//cmd filtering.~~
	if( !(epd_iriver_cmd == EPD_IRIVER_CMD_FULL
		|| epd_iriver_cmd == EPD_IRIVER_CMD_PART
		|| epd_iriver_cmd == EPD_IRIVER_CMD_FULL_AREA
		|| epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA
		|| epd_iriver_cmd == EPD_IRIVER_CMD_PART_BW
		|| epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA_BW
		|| epd_iriver_cmd == EPD_ROTATION_0
		|| epd_iriver_cmd == EPD_ROTATION_90
		|| epd_iriver_cmd == EPD_ROTATION_180
		|| epd_iriver_cmd == EPD_ROTATION_270
		))
	{		
		if(epd_iriver_cmd == EPD_IRIVER_CMD_BOOTING
			|| epd_iriver_cmd == EPD_IRIVER_CMD_REBOOT)
		{
			dbg_info("%s(), return, epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);
		}
#ifdef EPD_ENABLE_SLEEP
		else
		{
			long time = 0;
			
			//check sleep time.
			time = jiffies  - epd_pre_time;
			time = jiffies_to_msecs(time);

			if (get_cur_clock_state() == 2)
			{
				if(time > 300)//마지막 업데이트 1.5초 후에 sleep진입.
				{
					//
					//auto sleep.
					//
					s1d13521fb_goto_sleep();
					epd_pre_time = 0;
				}

			}
			else
			{
				if(time > 1300)//마지막 업데이트 1.5초 후에 sleep진입.
				//if(time > 1500)//마지막 업데이트 1.5초 후에 sleep진입.
				{
					//
					//auto sleep.
					//
					s1d13521fb_goto_sleep();
					epd_pre_time = 0;
				}
			}
		}
#endif//EPD_ENABLE_SLEEP
		
		return;
	}

	if (get_cur_sleep_mode() == 5)
		printk("%s() epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);
		//dbg_info("%s() epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);

	//
	// system clock high.
	//
	force_high_clock();

	//
	//auto sys run.(power normal mode)
	//	
#ifdef EPD_ENABLE_SLEEP
	s1d13521fb_goto_normal();	
#endif//EPD_ENABLE_SLEEP

	if(s1d13521if_WaitForHRDY()<0)
	{
		error_need_epd_reboot = 1;
		s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);

		return;
	}

	//check epd engine.
	
	if(wait_for_lut_free()<0)
	{
		error_need_epd_reboot = 1;
		s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);

		return;
	}
	if(wait_for_trigger_busy()<0)
	{
		error_need_epd_reboot = 1;
		s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);

		return;
	}
	if(wait_for_display_frame_busy()<0)
	{
		error_need_epd_reboot = 1;
		s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);

		return;
	}

	epd_iriver_cmd = s1d13521fb_check_rotation(epd_iriver_cmd);
			
	//
	// once in a while do an accurate update... 
	//
	if(epd_iriver_cmd == EPD_IRIVER_CMD_PART)
	{
#ifdef SUPPORT_16_GRAY		
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;//current.
#endif
		cmd = UPD_PART;

		s1d13521fb_do_refresh_display(cmd, mode);
		//dbg_info("k-> EPD_IRIVER_CMD_PART!\n");
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_PART_BW)
	{
		mode = WF_MODE_MU;
		cmd = UPD_PART;

		s1d13521fb_do_refresh_display(cmd, mode);
		//dbg_info("k-> EPD_IRIVER_CMD_PART_BW!\n");
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_FULL)
	{
#ifdef SUPPORT_16_GRAY			
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL;

		s1d13521fb_do_refresh_display(cmd, mode);
		//dbg_info("k-> EPD_IRIVER_CMD_FULL!\n");
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA)
	{
		int x, y, width, height;
		int odd_check;
		
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_PART_AREA;
		
		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_PART_AREA!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA_BW)
	{
		int x, y, width, height;
		int odd_check;
		
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
	
		mode = WF_MODE_MU;
		cmd = UPD_PART_AREA;
		
		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_PART_AREA_BW!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_FULL_AREA)
	{
		int x, y, width, height;
		int odd_check;
	
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL_AREA;

		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_FULL_AREA!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}

#ifdef EPD_ENABLE_SLEEP
	//read_temperature_value();

	//
	//auto sleep.
	//
//	s1d13521fb_goto_sleep();
	epd_pre_time = jiffies;
#endif//EPD_ENABLE_SLEEP

	//
	//init cmd.
	//
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);


	//woong
	//set_readyToChangeClock(1);
}


static DECLARE_WORK(s1d13521_work, (void *)&s1d13521fb_refresh_display);

void s1d13521_timer_function(unsigned long data)
{
	schedule_work(&s1d13521_work);

	mod_timer(&s1d13521_timer, jiffies+VIDEO_REFRESH_PERIOD);
}

#endif


#ifdef BOOTING_REFRESH_PERIOD
static void s1d13521fb_refresh_booting_img(unsigned long dummy)
{
	int epd_iriver_cmd = s1d13521fb_get_virtual_framebuffer_cmd();
	unsigned mode, cmd =0x00;

	//
	// HIRQ
	//
	if(get_elice_board_version() == ELICE_BOARD_WS1)
	{
		//폴링으로 체트함. WS보드에서만 사용.
		if(gpio_get_value(S3C64XX_GPM(3)) >0)
		{
			s1d13521fb_hirq_isr_bh(NULL);
		}
	}

	//cmd filtering.~~
	if( epd_iriver_cmd != EPD_IRIVER_CMD_BOOTING)
	{	
		return;
	}

	epd_iriver_cmd = EPD_IRIVER_CMD_PART_AREA_BW;
	
	//dbg_info("%s() epd_iriver_cmd: 0x%x\n",__FUNCTION__, epd_iriver_cmd);

	//check epd engine.
	wait_for_lut_free();
	wait_for_trigger_busy();
	wait_for_display_frame_busy();  	

	epd_iriver_cmd = s1d13521fb_check_rotation(epd_iriver_cmd);
			
	//
	// once in a while do an accurate update... 
	//
	if(epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA_BW)
	{
		int x, y, width, height;
		int odd_check;
		
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
	
		mode = WF_MODE_MU;
		cmd = UPD_PART_AREA;
		
		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_PART_AREA_BW!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_PART)
	{
		mode = WF_MODE_GU;
		cmd = UPD_PART;

		s1d13521fb_do_refresh_display(cmd, mode);
		//dbg_info("k-> EPD_IRIVER_CMD_PART!\n");
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_FULL)
	{
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL;

		s1d13521fb_do_refresh_display(cmd, mode);
		//dbg_info("k-> EPD_IRIVER_CMD_FULL!\n");
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_PART_AREA)
	{
		int x, y, width, height;
		int odd_check;
		
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
	
		mode = WF_MODE_GU;
		cmd = UPD_PART_AREA;
		
		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_PART_AREA!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}
	else if(epd_iriver_cmd == EPD_IRIVER_CMD_FULL_AREA)
	{
		int x, y, width, height;
		int odd_check;
	
		x = s1d13521fb_get_virtual_framebuffer_area_x();
		y = s1d13521fb_get_virtual_framebuffer_area_y();
		width = s1d13521fb_get_virtual_framebuffer_area_width();
		height = s1d13521fb_get_virtual_framebuffer_area_height();

		odd_check = width%2;
		if(odd_check)
		{
			width++;
		}
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL_AREA;

		s1d13521fb_do_area_refresh_display(cmd, mode, x, y, width, height);

		//dbg_info("k-> EPD_IRIVER_CMD_FULL_AREA!(x: %d, y: %d, width: %d, height: %d)\n", x, y, width, height);
	}




	//init cmd.
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);
}


static DECLARE_WORK(s1d13521_booting, (void *)&s1d13521fb_refresh_booting_img);

int boot_loading_bar_pos = 0;
void s1d13521_timer_booting_function(unsigned long data)
{
	if ( ( s1d13521fb_get_virtual_framebuffer_cmd() != EPD_IRIVER_CMD_INIT ) )
	{
		mod_timer(&s1d13521_booting_timer, jiffies+BOOTING_REFRESH_PERIOD);
		return;
	}

	//if(boot_loading_bar_pos > 400)
	if(boot_loading_bar_pos > 335)
	{
		del_timer_sync(&s1d13521_booting_timer);
		return;
	}

	dbg_info("[boot_loading_bar_pos: %d (0~373)]\n", boot_loading_bar_pos);

	if(boot_loading_bar_pos > 373)
	{
		boot_loading_bar_pos = 373;
	}
	
	//ui_draw_fill_box(0x60, 177, 619, boot_loading_bar_pos, 3);
	ui_draw_fill_box(0x00, 177, 619, boot_loading_bar_pos, 3);
	//ui_draw_fill_box(0xff, 177, 619, boot_loading_bar_pos, 3);
	s1d13521fb_set_virtual_framebuffer_area(177, 619, boot_loading_bar_pos, 3);
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_BOOTING);
	boot_loading_bar_pos+=55;		

	schedule_work(&s1d13521_booting);
	mod_timer(&s1d13521_booting_timer, jiffies+BOOTING_REFRESH_PERIOD);
}
#endif//BOOTING_REFRESH_PERIOD







//----------------------------------------------------------------------------
// PRIVATE FUNCTION:
// One time display initialization.
//----------------------------------------------------------------------------
void s1d13521fb_init_display(void)
{
#if 0
	s1d13521_ioctl_cmd_params cmd_params;
	u32     i,size16 = VFB_SIZE_BYTES/2;
	u16*    pSource = (u16*) s1d13521fb_info.VirtualFramebufferAddr;
	u32*    p32 = (u32*) pSource;

	dbg_info("%s()++\n",__FUNCTION__);

	// Copy virtual framebuffer to hardware via indirect interface burst mode write
	for (i = 0; i < VFB_SIZE_BYTES/4; i++)
	//for (i = 0; i < VFB_SIZE_BYTES/16; i++)
	{
		*p32++ = 0xFFFFFFFF;    // white
	}

	ui_draw_logo();
	ui_draw_fill_box(0xc0, 177, 619, 373, 3);
	

	s1d13521if_WriteReg16(0x330,0x84);              // LUT AutoSelect+P4N, display engine software reset.

	mdelay(10);
	
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);

	mdelay(10);

#ifdef CONFIG_FB_EPSON_ENABLE_4BPP
	cmd_params.param[0] = (0x2<<4);// 4bit per pixel.(billy check, 0x2<<4하면 4bpp)
#else	
	cmd_params.param[0] = (0x3 << 4);
#endif//CONFIG_FB_EPSON_ENABLE_4BPP
	s1d13521if_cmd(LD_IMG,&cmd_params,1);

	mdelay(10);

// frame buffer write.	
	cmd_params.param[0] = 0x154;
	s1d13521if_cmd(WR_REG,&cmd_params,1);
	s1d13521if_BurstWrite16(pSource,size16);
	s1d13521if_cmd(LD_IMG_END,&cmd_params,0);

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	cmd_params.param[0] = ((WF_MODE_INIT<<8) |0x4000);//display update wave form mode, border update enable.
	s1d13521if_cmd(UPD_FULL,&cmd_params,1);         // update all pixels

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

/*
	//billy add
	s1d13521if_cmd(UPD_INIT,&cmd_params,0);         // update all pixels

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!
*/	
	mdelay(5);

	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	s1d13521if_cmd(WAIT_DSPE_FREND,&cmd_params,0);

	dbg_info("%s()--\n",__FUNCTION__);
#endif
	s1d13521_ioctl_cmd_params cmd_params;
	u32     i,size16 = VFB_SIZE_BYTES/2;
	u16*    pSource = (u16*) s1d13521fb_info.VirtualFramebufferAddr;
	u32*    p32 = (u32*) pSource;

	dbg_info("%s():\n",__FUNCTION__);

//billy add
	s1d13521if_WaitForHRDY();
	//check epd engine.
	wait_for_lut_free();
	mdelay(1);
	wait_for_trigger_busy();
	wait_for_display_frame_busy();  
	s1d13521if_WaitForHRDY();
	mdelay(1);
//end billy add

	// Copy virtual framebuffer to hardware via indirect interface burst mode write
	for (i = 0; i < VFB_SIZE_BYTES/4; i++)
	        *p32++ = 0xFFFFFFFF;    // white

	s1d13521if_WriteReg16(0x330,0x84);              // LUT AutoSelect+P4N
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	cmd_params.param[0] = (0x3 << 4);
	s1d13521if_cmd(LD_IMG,&cmd_params,1);
	cmd_params.param[0] = 0x154;
	s1d13521if_cmd(WR_REG,&cmd_params,1);
	s1d13521if_BurstWrite16(pSource,size16);
	s1d13521if_cmd(LD_IMG_END,&cmd_params,0);
	//cmd_params.param[0] = ((WF_MODE_INIT<<8) |0x4000);
	//s1d13521if_cmd(UPD_FULL,&cmd_params,1);         // update all pixels

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	cmd_params.param[0] = 0x00;
	s1d13521if_cmd(UPD_INIT,&cmd_params,0);         // init
	
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	s1d13521if_cmd(WAIT_DSPE_FREND,&cmd_params,0);

	dbg_info("%s()--\n",__FUNCTION__);
}





void ui_draw_16gray(void)//0xf0, 0xa0, 0x50, 0x00
{
	int i, j;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

#if 1
	for (j = 0; j < S1D_DISPLAY_HEIGHT; j++)
	{
		for(i = 0; i < S1D_DISPLAY_WIDTH; i++)
		{
			if(j<50*1)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xf0;//white.
			}
			else if(j<50*2)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xe0;
			}
			else if(j<50*3)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xd0;
			}
			else if(j<50*4)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xc0;
			}
			else if(j<50*5)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xb0;
			}
			else if(j<50*6)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xa0;//
			}
			else if(j<50*7)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x90;
			}
			else if(j<50*8)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x80;
			}
			else if(j<50*9)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x70;
			}
			else if(j<50*10)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x60;
			}
			else if(j<50*11)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x50;//
			}
			else if(j<50*12)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x40;;
			}
			else if(j<50*13)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x30;
			}
			else if(j<50*14)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x20;
			}
			else if(j<50*15)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x10;
			}
			else
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0x00;//black
			}
		}
	}
#endif//0
}


void s1d13521fb_white_display(int update_mode, int color_type)//0: full update, 1: bw full update. || 0:white, 1:black.
{
#ifdef SUPPORT_16_GRAY	
	unsigned mode = WF_MODE_GU, cmd = UPD_FULL;
#else
	unsigned mode = WF_MODE_GC, cmd = UPD_FULL;
#endif
	int i;
	u16*    pSource = (u16*) s1d13521fb_info.VirtualFramebufferAddr;
	u32*    p32 = (u32*) pSource;
	u32 color = 0xFFFFFFFF;

	dbg_info("%s()\n",__FUNCTION__);

	if(color_type == 0)
	{
		//white.
		color = 0xFFFFFFFF;
	}
	else if(color_type == 1)
	{
		//black.
		color = 0x00000000;
	}
	// Copy virtual framebuffer to hardware via indirect interface burst mode write
	for (i = 0; i < VFB_SIZE_BYTES/4; i++)
	{
		*p32++ = color;    
	}	

	s1d13521if_WaitForHRDY();

	//check epd engine.
	wait_for_lut_free();
	mdelay(1);
	wait_for_trigger_busy();
	wait_for_display_frame_busy();  
	s1d13521if_WaitForHRDY();
	mdelay(1);

	if(update_mode == 0)//full update.
	{
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL;
	}
	else if(update_mode == 1)//bw update.
	{
		mode = WF_MODE_MU;
		//cmd = UPD_PART;
		cmd = UPD_FULL;
	}
	else if(update_mode == 2)//bw update.
	{
		mode = WF_MODE_MU;
		cmd = UPD_PART;
		//cmd = UPD_FULL;
	}

	s1d13521fb_do_refresh_display(cmd,mode);     

	mdelay(4);

	//init cmd..
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);


}




#include <plat/regs-clock.h>
// tmp
// 0 : power switch, 또는 모든 power off 상황
// 1 : reboot
// 2 : rtc on
// 3 : 충전 케이블
// tmp = __raw_readl(S3C_INFORM2);
extern int check_battery_level_for_bootup(int sequence);
int s1d13521fb_logo_display(int bProgress, int update_type)//0: full, 1:part.
{
	unsigned mode,cmd;
	int battery_no = 0;
	
	dbg_info("%s()++\n",__FUNCTION__);

	//draw image.
	if(check_battery_level_for_bootup(1))
	{
		//woong
		return 0;

		//low battery.
		ui_draw_battery_no();

		battery_no = 1;
	}
	else
	{
		ui_draw_logo();
		if(bProgress)
		{
			ui_draw_fill_box(0xc0, 177, 619, 373, 3);
		}
	}
	
	
/* for test.
	ui_draw_fill_box(0x00, 177, 10, 373, 10);
	ui_draw_fill_box(0x20, 177, 20, 373, 10);
	ui_draw_fill_box(0x40, 177, 30, 373, 10);
	ui_draw_fill_box(0x60, 177, 40, 373, 10);
	ui_draw_fill_box(0x80, 177, 50, 373, 10);
	ui_draw_fill_box(0xa0, 177, 60, 373, 10);
	ui_draw_fill_box(0xc0, 177, 70, 373, 10);
	ui_draw_fill_box(0xe0, 177, 80, 373, 10);
	ui_draw_fill_box(0x00, 177, 90, 373, 10);
*/
	s1d13521if_WaitForHRDY();

	//check epd engine.
	wait_for_lut_free();
	mdelay(1);
	wait_for_trigger_busy();
	wait_for_display_frame_busy();  
	s1d13521if_WaitForHRDY();
	mdelay(1);

	if(update_type)
	{
#ifdef SUPPORT_16_GRAY			
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_PART;
	}
	else
	{
#ifdef SUPPORT_16_GRAY	
		mode = WF_MODE_GU;
#else
		mode = WF_MODE_GC;
#endif
		cmd = UPD_FULL;
	}

	s1d13521fb_do_refresh_display(cmd,mode);     

	mdelay(4);

	//init cmd..
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_INIT);
	dbg_info("%s()--\n",__FUNCTION__);

	if(battery_no)
	{
		return 0;
	}
	
	return 1;
}



//
// for epd power off!
//
void ebook_external_poweroff_display(void)
{
	//
	//kill epd display timer.
	//
	#ifdef VIDEO_REFRESH_PERIOD
	del_timer_sync(&s1d13521_timer);
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_POWEROFF);
	#endif//VIDEO_REFRESH_PERIOD

	//
	//auto sys run.(power normal mode)
	//
	if(is_s1d13521fb_sleep_mode())
	{
		//goto sleep mode.
		s1d13521fb_goto_normal();

		s1d13521if_WaitForHRDY();
	}

	//s1d13521fb_logo_display(false, 0);
	s1d13521fb_white_display(0, 0);//full, white.
	
	dbg_info("ebook_external_poweroff_display\n");

	//check HRDY
	dbg_info("ebook_external_poweroff_display(), waiting HRDY\n");
	s1d13521if_WaitForHRDY();
	//dbg_info("ebook_external_poweroff_display(), end waiting HRDY\n");

	//check epd engine.
	wait_for_lut_free();
	dbg_info("ebook_external_poweroff_display(), ok, lut free\n");
	mdelay(1);
	wait_for_trigger_busy();
	dbg_info("ebook_external_poweroff_display(), ok, trigger busy!\n");
	wait_for_display_frame_busy(); 
	dbg_info("ebook_external_poweroff_display(), ok, display frame busy!\n");
	s1d13521if_WaitForHRDY();
	
}
EXPORT_SYMBOL_GPL(ebook_external_poweroff_display);

#if 0
int wait_for_power_cycle_busy( void )
{
	int cnt = 1000;
	int v = s1d13521if_ReadReg16( 0x0230 );
	
	while ( ( ( v >> 7 ) & 0x1 ) != ( 0 & 0x1 ) )
	{
		v = s1d13521if_ReadReg16( 0x0230 );
		if ( v != 0xFFFF ) break;
		dbg_info("%s() now waiting!!\n",__FUNCTION__);

		if(cnt<0)
		{
			return -1;
		}
		cnt--;

		udelay(100);
	} // while

	return 1;
}
#endif

void ebook_external_ready_poweroff(void)
{

#if 0
	u16 read_val;

	//read_val = s1d13521if_ReadReg16( 0x0230 );
	//printk("state: 0x%x #1\n", read_val);

	//read_val = s1d13521if_ReadReg16( 0x0232 );
	//printk("ebook_external_ready_poweroff: 0x%x #1\n", read_val);

	//pwr 2, 3
	read_val = 0x1e06;
	s1d13521if_WriteReg16( 0x0232, read_val );
	//printk("ebook_external_ready_poweroff: 0x%x #1\n", read_val);

	//read_val = s1d13521if_ReadReg16( 0x0230 );
	//printk("pwr2, 3: state: 0x%x #1\n", read_val);
	mdelay(1000);


	//pwr 1
	read_val = 0x1e02;
	s1d13521if_WriteReg16( 0x0232, read_val );
	//printk("ebook_external_ready_poweroff: 0x%x #2\n", read_val);

	//read_val = s1d13521if_ReadReg16( 0x0230 );
	//printk("pwr1: state: 0x%x #1\n", read_val);
	mdelay(1000);


	//pwr 0
	read_val = 0x1e00;
	s1d13521if_WriteReg16( 0x0232, read_val );
	//printk("ebook_external_ready_poweroff: 0x%x #3\n", read_val);

	//read_val = s1d13521if_ReadReg16( 0x0230 );
	//printk("pwr0: state: 0x%x #1\n", read_val);
	mdelay(1000);

#endif

	s1d13521fb_goto_sleep();

	mdelay(1000);
	//mdelay(3000);
}
EXPORT_SYMBOL_GPL(ebook_external_ready_poweroff);






void s1d13521fb_gray_display(unsigned char GrayVal)
{
	int i, j;
	char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

	dbg_info("%s(), GrayVal:0x%x\n",__FUNCTION__, GrayVal);

	for (j = 0; j < S1D_DISPLAY_HEIGHT; j++)
	{
		for(i = 0; i < S1D_DISPLAY_WIDTH; i++)
		{
			*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = GrayVal;
		}
	}

	{
	        unsigned mode,cmd;

	        // once in a while do an accurate update... 
	        {
#ifdef SUPPORT_16_GRAY					
				mode = WF_MODE_GU;
#else
	                mode = WF_MODE_GC;
#endif
	                cmd = UPD_FULL;
	//                gGCCount = VFB_GC_COUNT;
	        }

	        s1d13521fb_do_refresh_display(cmd,mode);
	}


}



int s1d13521fb_print_wfm_info( void )
{
	int wfm_fvsn = 0;
	int wfm_luts = 0;
	int wfm_mc = 0;
	int wfm_trc = 0;
	int wfm_eb = 0;
	int wfm_sb = 0;
	int wfm_wmta = 0;

	u16 a = s1d13521if_ReadReg16( 0x354 );
	u16 b = s1d13521if_ReadReg16( 0x356 );
	u16 c = s1d13521if_ReadReg16( 0x358 );
	u16 d = s1d13521if_ReadReg16( 0x35C );
	u16 e = s1d13521if_ReadReg16( 0x35E );
	wfm_fvsn = a & 0xFF;
	wfm_luts = ( a >> 8 ) & 0xFF;
	wfm_trc = ( b >> 8 ) & 0xFF;
	wfm_mc = b & 0xFF;
	wfm_sb = ( c >> 8 ) & 0xFF;
	wfm_eb = c & 0xFF;
	wfm_wmta = d | ( e << 16 );
  
	printk( "[%s] wfm: fvsn=%d luts=%d mc=%d trc=%d eb=0x%02x sb=0x%02x wmta=%d\n",
		__FUNCTION__, wfm_fvsn, wfm_luts, wfm_mc, wfm_trc, wfm_eb, wfm_sb, wfm_wmta );

	if(/*wfm_fvsn==0 && */wfm_luts==0 && wfm_mc==0 && wfm_trc == 0 && wfm_eb==0 && wfm_sb == 0 && wfm_wmta == 0)
	{
		//no data.
		return 0;
	}

	return 1;
}


int get_pll_state(void)
{
	//pll configuration 0
	int v = s1d13521if_ReadReg16( 0x010 );
	dbg_info("%s() pll configuration 0 register: 0x%x\n(? 0x03 for input clock 25MHz)\n",__FUNCTION__, v);// 0x03
	if(v != 0x03)
	{
		dbg_info("%s(), [fail!!] pll configuration 0: 0x%x\n\n",__FUNCTION__, v);
		return -1;
	}

	//pll configuration 1
	v = s1d13521if_ReadReg16( 0x012 );
	dbg_info("%s() pll configuration 1 register: 0x%x\n(? 0x5949 for input clock 25MHz)\n",__FUNCTION__, v);//0x5949
	if(v != 0x5949)
	{
		dbg_info("%s(), [fail!!] pll configuration 1: 0x%x\n\n",__FUNCTION__, v);
		return -1;
	}

	//pll configuration 2
	v = s1d13521if_ReadReg16( 0x014 );
	dbg_info("%s() pll configuration 2 register: 0x%x\n(? 0x40 for input clock 25MHz)\n",__FUNCTION__, v);//0x40
	if(v != 0x40)
	{
		dbg_info("%s(), [fail!!] pll configuration 2: 0x%x\n\n",__FUNCTION__, v);
		return -1;
	}

	//clock configuration.
	v = s1d13521if_ReadReg16( 0x016 );
	dbg_info("%s() clock configuration register: 0x%x(? 0x00)\n",__FUNCTION__, v);//0x00

	//pixel clock configuration.
	v = s1d13521if_ReadReg16( 0x018 );
	dbg_info("%s() pixel clock configuration register: 0x%x \n(? 0x06 for divide ration 7:1)\n",__FUNCTION__, v);//0x06

	//I2C Thermal Sensor clock configuration.
	v = s1d13521if_ReadReg16( 0x01a );
	dbg_info("%s() I2C Thermal Sensor  clock configuration register[REG(0x01a)]: 0x%x(0x0a -> 0x04)\n",__FUNCTION__, v);//0x0a

	return 1;
}



int check_for_pll_stable(void)
{
	int cnt = 50;//50ms
	
	int v = s1d13521if_ReadReg16( 0x00a );
	
	while ( ( ( v >> 0 ) & 0x1 ) != 0x0001 )
	{
		 mdelay(1);

	        if (--cnt <= 0)         // Avoid endless loop
	        {       
	                printk(KERN_ERR "[fail!!!] check_for_pll_stable\n");
	                return -1;
	        }
			
		v = s1d13521if_ReadReg16( 0x00a );
		//dbg_info("%s() now waiting!!\n",__FUNCTION__);
	}

	dbg_info("%s() , epd pll stable!!\n",__FUNCTION__);
	return 1;
}


int check_for_sdram_complete(void)
{
	int cnt = 50;//50ms
	
	int v = s1d13521if_ReadReg16( 0x102 );
	
	while ( ( ( v >> 8 ) & 0x1 ) != 0x0001 )
	{
		 mdelay(1);

	        if (--cnt <= 0)         // Avoid endless loop
	        {       
	                printk(KERN_ERR "[fail!!!] check_for_sdram_complete\n");
	                return -1;
	        }
			
		v = s1d13521if_ReadReg16( 0x102 );
		//dbg_info("%s() now waiting!!\n",__FUNCTION__);
	}

	dbg_info("%s() , epd sdram initialization complete!!\n",__FUNCTION__);
	return 1;
}


#if 0//test function.
void read_temperature_value(void)
{
	int v, ic_tem;
#if 0
	int cnt = 200;//50ms

	v = s1d13521if_ReadReg16( 0x210 );
	dbg_info("[k, epd] Reg[0x210]: 0x%x(Thermal Sensor config)\n", v);

	s1d13521if_WriteReg16(0x214, 0x01);

	v = s1d13521if_ReadReg16( 0x212 );
	dbg_info("[k, epd] Reg[0x212]: 0x%x(Thermal Sensor Status)\n", v);
	
	while ( ( v & 0x1 ) != 0x0001 )
	{
		 mdelay(1);

	        if (--cnt <= 0)         // Avoid endless loop
	        {       
	                printk(KERN_ERR "[fail!!!] i2c thermal busy!!!, wait: %d msec\n", cnt);
	                //return -1;
	                return;
	        }
			
		v = s1d13521if_ReadReg16( 0x212 );
		//dbg_info("%s() reg[0x212]: 0x%x\n",__FUNCTION__, v);
	}

	//v = s1d13521if_ReadReg16( 0x216 );
	ic_tem = s1d13521if_ReadReg16( 0x216 );
	//dbg_info("%s() ,reg[0x216]: 0x%x, IC temperature val: %d C.\n",__FUNCTION__, v, v);

	//v = s1d13521if_ReadReg16( 0x320 );
	//dbg_info("%s() ,reg[0x320]: 0x%x(? 0x00)\n",__FUNCTION__, v);


	v = s1d13521if_ReadReg16( 0x322 );
	dbg_info("[k, epd] REG[0x322]: epson temperature: %d C.\n[k, epd] REG[0x216]: IC temperature: %d C.\n", v, ic_tem);

	//I2C Thermal Sensor clock configuration.
	v = s1d13521if_ReadReg16( 0x01a );
	dbg_info("[k, epd] I2C Thermal Sensor clock configuration[REG(0x01a)]: 0x%x(? 0x0a)\n", v);

	v = s1d13521if_ReadReg16( 0x320 );
	dbg_info("[k, epd] Thermal device select[REG(0x320)]: 0x%x(? 0x00)\n", v);
#endif 

	v = s1d13521if_ReadReg16( 0x322 );
	printk("[k, epd] REG[0x322]: epson temperature: %d C.\n", v);
}
#endif


int check_for_temperature_compensated_self_refresh(void)
{
	int v;
	
	v = s1d13521if_ReadReg16( 0x10A );
	dbg_info("%s() , reg[0x10A]: 0x%x\n",__FUNCTION__, v);//temperature compensaed self refresh.

	v = s1d13521if_ReadReg16( 0x210);
	dbg_info("%s() , reg[0x210]: 0x%x\n",__FUNCTION__, v);

	v = s1d13521if_ReadReg16( 0x212);
	dbg_info("%s() , reg[0x212]: 0x%x\n",__FUNCTION__, v);

	v = s1d13521if_ReadReg16( 0x320);
	dbg_info("%s() , reg[0x320]: 0x%x\n",__FUNCTION__, v);
	

//	read_temperature_value();
	
	return 1;
}


int check_for_waveform_mode(void)
{
	int v;
	
	v = s1d13521if_ReadReg16( 0x330);
	dbg_info("%s() , reg[0x330]: 0x%x\n",__FUNCTION__, v);

	return 1;
}


int disable_epd_HIRQ(void)
{
	int v;
	s1d13521if_WriteReg16(0x244, 0x0000);
	
	v = s1d13521if_ReadReg16( 0x244);
	dbg_info("%s() , reg[0x244]: 0x%x\n",__FUNCTION__, v);

	return 1;
}


/*
//boarder구성을 설정할수 있음. LGD의 경우 현재 boarder를 black으로 하고 있음.
int set_epd_border(void)
{
	int v;
		
	v = s1d13521if_ReadReg16( 0x326);
	printk("%s() , reg[0x326]: 0x%x\n",__FUNCTION__, v);

	s1d13521if_WriteReg16(0x326, 0x00ff);//white.

	v = s1d13521if_ReadReg16( 0x326);
	printk("%s() , reg[0x326]: 0x%x\n",__FUNCTION__, v);

	return 1;
}
*/


//-----------------------------------------------------------------------------
// Initialize the chip and the frame buffer driver.
//-----------------------------------------------------------------------------
int s1d13521fb_init(void)
{       
	int init_retry = 0;
	
        dbg_info("%s()++\n",__FUNCTION__); 

        // Initialize the chip to use Indirect Interface
        if (s1d13521if_InterfaceInit(&s1d13521fb_info) != 0)
        {
                printk(KERN_WARNING "s1d13521fb_init: InterfaceInit error\n"); 
                return -EINVAL;
        }     

	//
	// HIRQ, 현보드에서는 사용하지 않음, 차후 필요시 인터럽트 구현필요.
	//
	if(get_elice_board_version() == ELICE_BOARD_WS1)
	{
		s3c_gpio_cfgpin(S3C64XX_GPM(3), S3C_GPIO_INPUT);//input
		s3c_gpio_setpull(S3C64XX_GPM(3), S3C_GPIO_PULL_NONE);
	}
	else
	{
		//HIRQ전부 disable시킨다, NC pin처리, Output Low로 설정.
		s3c_gpio_cfgpin(S3C64XX_GPN(10), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S3C64XX_GPN(10), S3C_GPIO_PULL_DOWN);
	}

        s1d13521fb_set_par(&s1d13521_fb);

        // Allocate the virtual framebuffer
        if (s1d13521fb_set_virtual_framebuffer())
        {
                printk(KERN_WARNING "s1d13521fb_init: _get_free_pages() failed.\n");
                return -EINVAL;
        }

        //-------------------------------------------------------------------------
        // Set the controller registers and initialize the display
        //-------------------------------------------------------------------------
	if(s1d13521fb_InitRegisters()<0)
	{
		//fail!!, retry more....
		retry_count = 0;
		
		init_retry = 1;
	}
	else
	{
		init_retry = 0;
	}

//최대 10번까지 시도함, serial flash에 waveform이 없는 경우는 10번 fail이 나고, 커널 부팅이됨.
 retry:
	if(init_retry)
	{
		printk("%s(),[epd restart] retry_count: %d\n",__FUNCTION__, retry_count); 
		s1d13521if_InterfaceInit(&s1d13521fb_info);
		if(s1d13521fb_InitRegisters()<0 && retry_count<10)
		{
			printk("%s() [fail!], s1d13521fb_InitRegisters()\n",__FUNCTION__); 
			init_retry = 1;
			retry_count++;
			
			goto retry;
		}

		init_retry = 0;
	}

	mdelay(20);

	//check HRDY.
	if(s1d13521if_WaitForHRDY()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		printk("%s() HRDY busy!!\n",__FUNCTION__); 
		goto retry;
	}

	// check pll
	if(check_for_pll_stable()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}

	// check pll value
	if(get_pll_state()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}

	//check sdram.
	if(check_for_sdram_complete()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}
 
 	//print waveform info.
	if(s1d13521fb_print_wfm_info() == 0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		printk("%s() no waveform data!!\n",__FUNCTION__); 

		goto retry;
	}

	//s1d13521if_WriteReg16( 0x01A, 4 ); // i2c clock divider
	s1d13521if_WriteReg16( 0x01A, 5 ); // i2c clock divider
  	s1d13521if_WriteReg16( 0x320, 0 ); // temp auto read on

	//check temperature compensated self refresh bit.(0x10a, 11~12bit)
	check_for_temperature_compensated_self_refresh();

	//about waveform
	check_for_waveform_mode();

	//disable HIRQ(혹시 사용하게되면 이거 주석처리한다.)
	disable_epd_HIRQ();

	//-------------------------------------------------------------------------
	// Initialize Hardware Display Blank
	//-------------------------------------------------------------------------
	s1d13521fb_info.blank_mode = VESA_NO_BLANKING;
	//s1d13521fb_ops.fb_blank = s1d13521fb_blank;
	s1d13521fb_ops.fb_blank = 0;// no use blank, billy

	// Set flags for controller supported features            
	s1d13521_fb.flags = FBINFO_FLAG_DEFAULT;

	// Set the pseudo palette        
	s1d13521_fb.pseudo_palette = s1d13521fb_info.pseudo_palette;

	s1d13521fb_check_var(&s1d13521_fb.var, &s1d13521_fb);
	s1d13521_fb.fbops = &s1d13521fb_ops;
	s1d13521_fb.node = -1;

	//border config.
	//set_epd_border();

	s1d13521fb_init_display();
	//main logo display.
	//s1d13521fb_white_display(0, 1);//full, black.
	//s1d13521fb_white_display(1, 0);//bw update, white.
	if(!s1d13521fb_logo_display(true, 0))//full update.
	{
		//power off, low battery
		//led off.
		gpio_set_value(S3C64XX_GPL(13), 0);

		//speak off.
		s3c_gpio_cfgpin(S3C64XX_GPK(3), S3C_GPIO_OUTPUT);
		gpio_set_value(S3C64XX_GPK(3), 0);
		//mute.
		s3c_gpio_cfgpin(S3C64XX_GPK(4), S3C_GPIO_OUTPUT);
		gpio_set_value(S3C64XX_GPK(4), 1);

		//for display.
		//ebook_external_poweroff_display();
		//mdelay(100);
		ebook_external_ready_poweroff();

		mdelay(1000);
		s3c_gpio_cfgpin(S3C64XX_GPK(6), S3C_GPIO_INPUT);//s3c_gpio_cfgpin(S3C_GPK6, S3C_GPK6_INP);
		s3c_gpio_setpull(S3C64XX_GPK(6), S3C_GPIO_PULL_DOWN);
		return 0;
	}

#if 0	//090703 for sleep test.
{
	unsigned cmd,numparams;
	s1d13521_ioctl_cmd_params cmd_params;

	s1d13521if_WaitForHRDY();
		
	cmd = SLP;//sleep.
	numparams = 0;
	cmd_params.param[0] = 0;
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	if(s1d13521if_reg_cmd(cmd,&cmd_params,numparams)<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		//return -1;
	}

	dbg_info("%s(), sleep ok!!\n",__FUNCTION__); 
}
#endif//090703 for sleep test.


//#ifdef VIDEO_REFRESH_PERIOD
#if 1//090703 for sleep test.
        // Create a timer to trigger a refresh of the displayed image
        init_timer(&s1d13521_timer);
        //s1d13521_timer.function = s1d13521fb_refresh_display;
        s1d13521_timer.function = s1d13521_timer_function;
        s1d13521_timer.expires = jiffies+VIDEO_REFRESH_PERIOD;
        //s1d13521_timer.expires = jiffies+HZ*3;
        add_timer(&s1d13521_timer);
#endif

//090703 for sleep test.
#if 1//booting bar timer.
	init_timer(&s1d13521_booting_timer);
        //s1d13521_timer.function = s1d13521fb_refresh_display;
        s1d13521_booting_timer.function = s1d13521_timer_booting_function;
        //s1d13521_timer.expires = jiffies+VIDEO_REFRESH_PERIOD;
        s1d13521_booting_timer.expires = jiffies+BOOTING_REFRESH_PERIOD;
        add_timer(&s1d13521_booting_timer);
#endif//booting

        if (register_framebuffer(&s1d13521_fb) < 0)
        {
                printk(KERN_ERR "s1d13521fb_init(): Could not register frame buffer with kernel.\n");
                return -EINVAL;
        }

        printk(KERN_INFO "fb%d: %s frame buffer device\n", s1d13521_fb.node, s1d13521_fb.fix.id);

#ifdef CONFIG_FB_EPSON_PROC
        s1d13521proc_init();
#endif

	dbg_info("%s()--\n",__FUNCTION__);
	return 0;
}



void s1d13521if_print_disp_timings(void)
{
	int vsize, vsync, vblen, velen, hsize, hsync, hblen, helen;

	vsize = s1d13521if_ReadReg16( 0x300 );
	vsync = s1d13521if_ReadReg16( 0x302 );
	vblen = s1d13521if_ReadReg16( 0x304 );
	velen = ( vblen >> 8 ) & 0xFF;
	vblen &= 0xFF;
	hsize = s1d13521if_ReadReg16( 0x306 );
	hsync = s1d13521if_ReadReg16( 0x308 );
	hblen = s1d13521if_ReadReg16( 0x30A );
	helen = ( hblen >> 8 ) & 0xFF;
	hblen &= 0xFF;
	
	printk( "[%s] disp_timings: vsize=%d vsync=%d vblen=%d velen=%d\n", __FUNCTION__, vsize, vsync, vblen, velen );
	printk( "[%s] disp_timings: hsize=%d hsync=%d hblen=%d helen=%d\n", __FUNCTION__, hsize, hsync, hblen, helen );
}



void s1d13521if_clear_gd(void)
{
	int v = ( 5 << 1 ) | 1;
	s1d13521if_WriteReg16( 0x0334, v );
	 
	bs_sfm_wait_for_bit( 0x338, 0, 0 );
	bs_sfm_wait_for_bit( 0x338, 3, 0 );
}


//about, gray_val, 
//=> -1 : framebuffer가공하지 않으므로 이전이미지 그대로 나옴, 
//=> 0x00~0xff : 0xff는 white, 0x00은 black, 중간값은 gray값을 화면에 나타나게함. 
int epson_epd_reboot(int gray_val)
{
	//reboot epd system.
	int cnt = 800, epd_state;
	u32     size16;
	u16*    pSource;
	s1d13521_ioctl_cmd_params cmd_params;
	//unsigned mode,cmd;
	int init_retry = 1;
	retry_count = 0;

	//
	//wait for state idle.
	epd_state = s1d13521fb_get_virtual_framebuffer_cmd();
	while ( !(epd_state == EPD_IRIVER_CMD_INIT) )
	{
		epd_state = s1d13521fb_get_virtual_framebuffer_cmd();
		dbg_info("%s() now waiting!!\n",__FUNCTION__);

		if(cnt<0)
		{
			break;
		}
		
		cnt--;
		mdelay(1);
	}

	//
	//start epd system reboot.
	//
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_REBOOT);
		
 retry:
	if(init_retry)
	{
		printk("%s(),[epd restart] retry_count: %d\n",__FUNCTION__, retry_count); 
		s1d13521if_InterfaceInit(&s1d13521fb_info);
		if(s1d13521fb_InitRegisters()<0 && retry_count<10)
		{
			printk("%s() [fail!], s1d13521fb_InitRegisters()\n",__FUNCTION__); 
			init_retry = 1;
			retry_count++;
			
			goto retry;
		}

		init_retry = 0;
	}

	mdelay(20);

	//check HRDY.
	if(s1d13521if_WaitForHRDY()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		printk("%s() HRDY busy!!\n",__FUNCTION__); 
		goto retry;
	}

	// check pll
	if(check_for_pll_stable()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}

	// check pll value
	if(get_pll_state()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}

	//check sdram.
	if(check_for_sdram_complete()<0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		
		goto retry;
	}
 
 	//print waveform info.
	if(s1d13521fb_print_wfm_info() == 0 && retry_count<10)
	{
		init_retry = 1;
		retry_count++;
		printk("%s() no waveform data!!\n",__FUNCTION__); 

		goto retry;
	}

	if(retry_count == 10)
	{
		return -1; //fail.
	}

	//s1d13521if_WriteReg16( 0x01A, 4 ); // i2c clock divider
	s1d13521if_WriteReg16( 0x01A, 5 ); // i2c clock divider
  	s1d13521if_WriteReg16( 0x320, 0 ); // temp auto read on

	size16 = VFB_SIZE_BYTES/2;
	pSource = (u16*) s1d13521fb_info.VirtualFramebufferAddr;

	s1d13521if_WaitForHRDY();
	//check epd engine.
	wait_for_lut_free();
	mdelay(1);
	wait_for_trigger_busy();
	wait_for_display_frame_busy();  
	s1d13521if_WaitForHRDY();
	mdelay(1);

	s1d13521if_WriteReg16(0x330,0x84);              // LUT AutoSelect+P4N
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	cmd_params.param[0] = (0x3 << 4);
	s1d13521if_cmd(LD_IMG,&cmd_params,1);
	cmd_params.param[0] = 0x154;
	s1d13521if_cmd(WR_REG,&cmd_params,1);
	s1d13521if_BurstWrite16(pSource,size16);
	s1d13521if_cmd(LD_IMG_END,&cmd_params,0);
	//cmd_params.param[0] = ((WF_MODE_INIT<<8) |0x4000);
	//s1d13521if_cmd(UPD_FULL,&cmd_params,1);         // update all pixels

	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

	cmd_params.param[0] = 0x00;
	s1d13521if_cmd(UPD_INIT,&cmd_params,0);         // init
	
	s1d13521if_cmd(WAIT_DSPE_TRG,&cmd_params,0);
	s1d13521if_cmd(WAIT_DSPE_FREND,&cmd_params,0);
	//finish driver init.

	//
	// draw ui
	//
	if(gray_val >= 0 && gray_val <= 0xff)
	{
		int i, j;
		char* p_buffer8 = (char*) s1d13521fb_info.VirtualFramebufferAddr;

		for (j = 0; j < S1D_DISPLAY_HEIGHT; j++)
		{
			for(i = 0; i < S1D_DISPLAY_WIDTH; i++)
			{
				*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = gray_val;
			}
		}
	}
	
	//
	// FULL update.
	//
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_FULL);

	//2009.11.08 : woong
	set_cur_epd_mode(0);
	
	return 1;
}
EXPORT_SYMBOL_GPL(epson_epd_reboot);



int s1d13521if_WaitForHRDY_in_reg(int wait_time);
//----------------------------------------------------------------------------
// PRIVATE FUNCTION:
// Set registers to initial values
//----------------------------------------------------------------------------
int s1d13521fb_InitRegisters(void)
{
        int ret=0;
        unsigned cmd,numparams;
        s1d13521_ioctl_cmd_params cmd_params;

	dbg_info("%s()++\n",__FUNCTION__); 

	//reset해야함.
	if(s1d13521if_WaitForHRDY_in_reg(200)<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	//billy추가 코드 am300 ref.
	s1d13521if_WriteReg16( 0x006, 0x0000 );//power save mode disable.
	s1d13521if_WriteReg16( 0x00a, 0x1000 );

	mdelay(100);

#if 1
/*
#define S1D_INSTANTIATE_REGISTERS(scope_prefix,variable_name)  \
  scope_prefix S1D_VALUE variable_name[] =  \
        { \
        INIT_SYS_RUN,   0,                                                      \
        INIT_DSPE_CFG,  5,      BS60_INIT_HSIZE,                                \
                                BS60_INIT_VSIZE,                                \
                                BS60_INIT_SDRV_CFG,                             \
                                BS60_INIT_GDRV_CFG,                             \
                                BS60_INIT_LUTIDXFMT,                            \
        INIT_DSPE_TMG,  5,      BS60_INIT_FSLEN,                                \
                                (BS60_INIT_FELEN<<8)|BS60_INIT_FBLEN,           \
                                BS60_INIT_LSLEN,                                \
                                (BS60_INIT_LELEN<<8)|BS60_INIT_LBLEN,           \
                                BS60_INIT_PIXCLKDIV,                            \
        RD_WFM_INFO,    2,      0x0886, 0,                                      \
        UPD_GDRV_CLR,   0,                                                      \
        WAIT_DSPE_TRG,  0,                                                      \
        INIT_ROTMODE,   1,      (BS60_INIT_ROTMODE << 8)                        \
        }
*/
	//INIT_SYS_RUN. 
	//initialize: PLL, SDRAM, DSPE(Display Setting), Turn on power.
	cmd = INIT_SYS_RUN;
	numparams = 0;
	cmd_params.param[0] = 0;
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	if (s1d13521if_WaitForHRDY_in_reg(200) != 0)
	{
		return -1;
	}

	//power pin delay setting.
	s1d13521if_WriteReg16(0x0234, 0x0018);//15msec
	s1d13521if_WriteReg16(0x0236, 0x008);//5msec
	s1d13521if_WriteReg16(0x0238, 0x0000);

	mdelay(10);

	if(check_for_pll_stable()<0)
	{
		dbg_info("%s(), fail!!(pll stable!!)\n",__FUNCTION__); 
		return -1;
	}

	if(get_pll_state()<0)
	{
		return -1;
	}


	//INIT_DSPE_CFG
	//Panel Settings, Initialize display engine.
	cmd = INIT_DSPE_CFG;
	numparams = 5;
	cmd_params.param[0] = BS60_INIT_HSIZE;
	cmd_params.param[1] = BS60_INIT_VSIZE;
	cmd_params.param[2] = BS60_INIT_SDRV_CFG;//source driver configuration register.
	cmd_params.param[3] = BS60_INIT_GDRV_CFG;
	cmd_params.param[4] = BS60_INIT_LUTIDXFMT;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}
	mdelay(5);

	//INIT_DSPE_TMG
	//Panel Settings, Initialize driver timings.
	cmd = INIT_DSPE_TMG;
	numparams = 5;
	cmd_params.param[0] = BS60_INIT_FSLEN;
	cmd_params.param[1] = (BS60_INIT_FELEN<<8)|BS60_INIT_FBLEN;
	cmd_params.param[2] = BS60_INIT_LSLEN;
	cmd_params.param[3] = (BS60_INIT_LELEN<<8)|BS60_INIT_LBLEN;
	cmd_params.param[4] = BS60_INIT_PIXCLKDIV;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}

	//billy추가 코드 am300 ref.
	s1d13521if_print_disp_timings( );

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}
	mdelay(5);//billy add

	//RD_WFM_INFO
 	//Read Waveform Infomation.
	cmd = RD_WFM_INFO;
	numparams = 2;
	cmd_params.param[0] = 0x0886;
	cmd_params.param[1] = 0x0000;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}
	mdelay(4);//billy add
	//s1d13521if_WaitForHRDY();
	//print waveform info.
	//s1d13521fb_print_wfm_info();

	//
	//sony 700UC ref add.
	//
	cmd = WAIT_DSPE_TRG;
	numparams = 0;
	cmd_params.param[0] = 0;
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}



	//
	//billy추가 코드 am300 ref.
	//
#if 0
	s1d13521if_clear_gd();
#else

	//UPD_GDRV_CLR
	//Gate Driver Clear Command.
	cmd = UPD_GDRV_CLR;
	numparams = 0;
	cmd_params.param[0] = 0;
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
#endif

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}
	mdelay(1);

//billy add
	bs_sfm_wait_for_bit( 0x338, 0, 0 );//Operation Trigger Busy Check!!
	bs_sfm_wait_for_bit( 0x338, 3, 0 );//Display Frame Busy Check!!

//	s1d13521if_WaitForHRDY();

	//WAIT_DSPE_TRG
	//wait for display engine trigger done.
	cmd = WAIT_DSPE_TRG;
	numparams = 0;
	cmd_params.param[0] = 0;
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}

	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		return -1;
	}
	mdelay(5);//billy add

	
	//INIT_ROTMODE
	//initialize rotation mode timings. , 최초 90도.
	cmd = INIT_ROTMODE;
	//numparams = 1;
	numparams = 1;
	cmd_params.param[0] = (BS60_INIT_ROTMODE << 8);
	cmd_params.param[1] = 0;
	cmd_params.param[2] = 0;
	cmd_params.param[3] = 0;
	cmd_params.param[4] = 0;
	ret = s1d13521if_reg_cmd(cmd,&cmd_params,numparams);
	if(ret<0)
	{
		dbg_info("%s(), fail!!(HRDY busy!!)\n",__FUNCTION__); 
		return -1;
	}
	
	//save cur mode.
	//cur_epd_rotation_mode = EPD_ROTATION_90; //evm.
	cur_epd_rotation_mode = EPD_ROTATION_270;	//ws ~~

#else
        S1D_INSTANTIATE_REGISTERS(static,InitCmdArray);

        i = 0;
        
        while (i < sizeof(InitCmdArray)/sizeof(InitCmdArray[0]))
        {
                cmd = InitCmdArray[i++];
                numparams = InitCmdArray[i++];

                for (j = 0; j < numparams; j++)
                        cmd_params.param[j] = InitCmdArray[i++];
        
                s1d13521if_cmd(cmd,&cmd_params,numparams);
        }
#endif

	dbg_info("%s()--\n",__FUNCTION__); 

	return 1;
}


//---------------------------------------------------------------------------- 
int s1d13521fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
        void __user *argp = (void __user *)arg;
        struct s1d13521_ioctl_hwc ioctl_hwc;
        s1d13521_ioctl_cmd_params cmd_params;

        //dbg_info("%s(): cmd=%04Xh\n", __FUNCTION__,cmd);
        
        switch (cmd)
        {
          // zero parameter commands:

          case S1D13521_RUN_SYS:
          case S1D13521_STBY:
          case S1D13521_SLP:
          case S1D13521_INIT_SYS_RUN:
          case S1D13521_INIT_SYS_STBY:
          case S1D13521_RD_SFM:
          case S1D13521_END_SFM:
          case S1D13521_BST_END_SDR:
          case S1D13521_LD_IMG_END:
          case S1D13521_LD_IMG_WAIT:
          case S1D13521_LD_IMG_DSPEADR:
          case S1D13521_WAIT_DSPE_TRG:  
          case S1D13521_WAIT_DSPE_FREND:
          case S1D13521_WAIT_DSPE_LUTFREE:      
          case S1D13521_UPD_INIT:
          case S1D13521_UPD_GDRV_CLR:
            s1d13521if_cmd(cmd,&cmd_params,0);
            break;

          // one parameter commands
          case S1D13521_INIT_ROTMODE:
          case S1D13521_WR_SFM:
          case S1D13521_LD_IMG:
          case S1D13521_WAIT_DSPE_MLUTFREE:
          case S1D13521_UPD_FULL:
          case S1D13521_UPD_PART:
            if (copy_from_user(&cmd_params, argp, 1*sizeof(u16)))
                return -EFAULT;

            s1d13521if_cmd(cmd,&cmd_params,1);  
            break;

          // two parameter commandss
          case S1D13521_WR_REG:
          case S1D13521_LD_IMG_SETADR:
          case S1D13521_RD_WFM_INFO:
          case S1D13521_UPD_SET_IMGADR:
            if (copy_from_user(&cmd_params, argp, 2*sizeof(u16)))
                return -EFAULT;

            if (cmd == S1D13521_WR_REG && cmd_params.param[0] == 0x154)
                s1d13521if_cmd(cmd,&cmd_params,1);      
            else                
                s1d13521if_cmd(cmd,&cmd_params,2);
        
            break;

          // three parameter commands           
          case S1D13521_INIT_CMD_SET:
          case S1D13521_INIT_PLL_STANDBY:
            if (copy_from_user(&cmd_params, argp, 3*sizeof(u16)))
                return -EFAULT;

            s1d13521if_cmd(cmd,&cmd_params,3);          
            break;

          // four parameter commands
          case S1D13521_INIT_SDRAM:
          case S1D13521_BST_RD_SDR:
          case S1D13521_BST_WR_SDR:
            if (copy_from_user(&cmd_params, argp, 4*sizeof(u16)))
                return -EFAULT;

            s1d13521if_cmd(cmd,&cmd_params,4);
            break;

          // five parameter commands            
          case S1D13521_INIT_DSPE_CFG:
          case S1D13521_INIT_DSPE_TMG:
          case S1D13521_LD_IMG_AREA:
          case S1D13521_UPD_FULL_AREA:
          case S1D13521_UPD_PART_AREA:
            if (copy_from_user(&cmd_params, argp, 5*sizeof(u16)))
                return -EFAULT;

            s1d13521if_cmd(cmd,&cmd_params,5);
            break;

          case S1D13521_RD_REG:
            if (copy_from_user(&cmd_params, argp, 2*sizeof(u16)))
                return -EFAULT;
           
            cmd_params.param[1] = s1d13521if_ReadReg16(cmd_params.param[1]);
            return copy_to_user(argp, &cmd_params, 2*sizeof(u16)) ? -EFAULT : 0;
                
          case S1D13521_REGREAD:
            if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
                return -EFAULT;

		printk("\n%s(): cmd=%04Xh, ioctl_hwc.addr: 0x%x\n", __FUNCTION__, cmd, ioctl_hwc.addr);
            ioctl_hwc.value = s1d13521if_ReadReg16(ioctl_hwc.addr);
		printk("\n%s(): cmd=%04Xh, ioctl_hwc.value: 0x%x\n", __FUNCTION__, cmd, ioctl_hwc.value);
            return copy_to_user(argp, &ioctl_hwc, sizeof(ioctl_hwc)) ? -EFAULT : 0;

          case S1D13521_REGWRITE:
            if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
                return -EFAULT;

		printk("\n%s(): cmd=%04Xh, ioctl_hwc.addr: 0x%x\n", __FUNCTION__, cmd, ioctl_hwc.addr);
            s1d13521if_WriteReg16(ioctl_hwc.addr, ioctl_hwc.value);
            return 0;

          case S1D13521_MEMBURSTWRITE:
            {
            u8 buffer[2048];
            unsigned user_buflen,copysize,copysize16;
            u16 *ptr16;
            u8* user_buffer;
                
            if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
                return -EFAULT;

            // ioctl_hwc.value = number of bytes in the user buffer
            // ioctl_hwc.buffer = pointer to user buffer
                
            user_buflen = ioctl_hwc.value;
            user_buffer = (u8*) ioctl_hwc.buffer;

            while (user_buflen != 0)
            {
                copysize = user_buflen;

                if (user_buflen > sizeof(buffer))                       
                        copysize = sizeof(buffer);

                if (copy_from_user(buffer,user_buffer,copysize))                
                        return -EFAULT;

                copysize16 = (copysize + 1)/2;
                ptr16 = (u16*) buffer;
                s1d13521if_BurstWrite16(ptr16,copysize16);
                user_buflen -= copysize;
                user_buffer += copysize;
            }

            return 0;
            }

          case S1D13521_MEMBURSTREAD:
            {
            u8 buffer[2048];
            unsigned user_buflen,copysize,copysize16;
            u16 *ptr16;
            u8* user_buffer;

            if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
                return -EFAULT;

            // ioctl_hwc.value = size in bytes of the user buffer, number of bytes to copy
            // ioctl_hwc.buffer = pointer to user buffer
                
            user_buflen = ioctl_hwc.value;
            user_buffer = (u8*) ioctl_hwc.buffer;

            while (user_buflen != 0)
            {
                copysize = user_buflen;

                if (user_buflen > sizeof(buffer))                       
                        copysize = sizeof(buffer);
        
                copysize16 = (copysize + 1)/2;
                ptr16 = (u16*) buffer;
                s1d13521if_BurstRead16(ptr16,copysize16);

                if (copy_to_user(user_buffer,buffer,copysize))          
                        return -EFAULT;

                user_buflen -= copysize;
                user_buffer += copysize;
            }

            return 0;           
            }

	case S1D13521_SFM_INIT:
	{
		printk("\n%s(): S1D13521_SFM_RESET\n", __FUNCTION__);

		//reset.
		s1d13521if_Reset();

		// 1. Init the clock
		bs_init_clk();

		// 2. Init the SPI block
		bs_init_spi();
		return 0;
	}

	case S1D13521_SFM_BULK_ERASE:
	{
		printk("\n%s(): S1D13521_SFM_BULK_ERASE\n", __FUNCTION__);
		epson_sfm_bulk_erase();
		return 0;
	}

	case S1D13521_SFM_WRITE_INIT:
	{
		printk("\n%s(): S1D13521_SFM_WRITE_INIT\n", __FUNCTION__);
		if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
			return -EFAULT;

		epson_ioctl_sfm_write_init(ioctl_hwc.addr);
		return 0;
	}
	
	case S1D13521_SFM_WRITE_DATA:
	{
		//printk("\n%s(): S1D13521_SFM_WRITE_DATA\n", __FUNCTION__);
		if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
			return -EFAULT;
//		printk("0x%02x \n", ioctl_hwc.value);
		bs_sfm_wr_byte(ioctl_hwc.value);
		return 0;
	}

	case S1D13521_SFM_WRITE_END:
	{
		printk("\n%s(): S1D13521_SFM_WRITE_END\n", __FUNCTION__);
		epson_ioctl_sfm_write_end();
		return 0;
	}

	case S1D13521_EPD_IRIVER_CMD:
	{		
		if (copy_from_user(&ioctl_hwc, argp, sizeof(ioctl_hwc)))
			return -EFAULT;

		printk("\n%s(): S1D13521_EPD_IRIVER_CMD, 0x%02x\n", __FUNCTION__, ioctl_hwc.value);

		
		while (  s1d13521fb_get_virtual_framebuffer_cmd() != EPD_IRIVER_CMD_INIT )
		{
			printk("\n%s(): wait!0x%02x\n", __FUNCTION__, ioctl_hwc.value);
			mdelay(10);
		}

		s1d13521fb_set_virtual_framebuffer_cmd((char)ioctl_hwc.value);
		return 0;
	}

	case S1D13521_EPD_IRIVER_REBOOT:
	{

#if 0
		//reboot epd system.
		int init_retry = 1;
		retry_count = 0;
			
	 retry:
		if(init_retry)
		{
			printk("%s(),[epd restart] retry_count: %d\n",__FUNCTION__, retry_count); 
			s1d13521if_InterfaceInit(&s1d13521fb_info);
			if(s1d13521fb_InitRegisters()<0 && retry_count<10)
			{
				printk("%s() [fail!], s1d13521fb_InitRegisters()\n",__FUNCTION__); 
				init_retry = 1;
				retry_count++;
				
				goto retry;
			}

			init_retry = 0;
		}

		mdelay(20);

		//check HRDY.
		if(s1d13521if_WaitForHRDY()<0 && retry_count<10)
		{
			init_retry = 1;
			retry_count++;
			
			printk("%s() HRDY busy!!\n",__FUNCTION__); 
			goto retry;
		}

		// check pll
		if(check_for_pll_stable()<0 && retry_count<10)
		{
			init_retry = 1;
			retry_count++;
			
			goto retry;
		}

		// check pll value
		if(get_pll_state()<0 && retry_count<10)
		{
			init_retry = 1;
			retry_count++;
			
			goto retry;
		}

		//check sdram.
		if(check_for_sdram_complete()<0 && retry_count<10)
		{
			init_retry = 1;
			retry_count++;
			
			goto retry;
		}
	 
	 	//print waveform info.
		if(s1d13521fb_print_wfm_info() == 0 && retry_count<10)
		{
			init_retry = 1;
			retry_count++;
			printk("%s() no waveform data!!\n",__FUNCTION__); 

			goto retry;
		}
#endif//0
		epson_epd_reboot(-1);

		return 0;
	}
		
	default:
            return -EINVAL;
			
        }

        return 0;
}

#if 0//billy
//---------------------------------------------------------------------------- 
// Blank the display
//
// >>> Modify powerup and powerdown sequences as required.          <<<
// >>> This routine will need to be extensively modified to support <<<
// >>> any powerdown modes required.                                <<<
//
//---------------------------------------------------------------------------- 
int s1d13521fb_blank(int blank_mode, struct fb_info *info)
{
        dbg_info("%s(): blank_mode=%d\n", __FUNCTION__, blank_mode);

        // If nothing has changed, just return
        if (s1d13521fb_info.blank_mode == blank_mode)
        {
                dbg_info("%s(): blank_mode=%d (not changed)\n", __FUNCTION__, blank_mode);
                return 0;
        }

        // Older versions of Linux Framebuffers used VESA modes, these are now remapped to FB_ modes
        switch (blank_mode)
        {
                // Disable display blanking and get out of powerdown mode.
                case FB_BLANK_UNBLANK: 
                        // If the last mode was powerdown, then it is necessary to power up.
                        if (s1d13521fb_info.blank_mode == FB_BLANK_POWERDOWN)
                        {
                        }

                        break;

                // Panels don't use VSYNC or HSYNC, but the intent here is just to blank the display
                case FB_BLANK_NORMAL: 
                case FB_BLANK_VSYNC_SUSPEND: 
                case FB_BLANK_HSYNC_SUSPEND:
			{
				dbg_info("%s(): FB_BLANK_NORMAL\n", __FUNCTION__);
                	}
                        break;

                case FB_BLANK_POWERDOWN:
                        // When powering down, the controller needs to get into an idle state
                        // then the power save register bits 0/1 are the key values to play with
                        // if sleep mode is used, it disables the PLL and will require 10 msec to 
                        // come back on. 
                        // Also, when powering down, the linux refresh timer should be stopped and 
                        // restarted when coming out of powerdown. 

                        // If the last mode wasn't powerdown, then powerdown here.
                        if (s1d13521fb_info.blank_mode != FB_BLANK_POWERDOWN)
                        {
                        }

                        break;

                default:
                        dbg_info("%s() dropped to default on arg %d\n", __FUNCTION__, blank_mode);
                        return -EINVAL;         // Invalid argument
        }

        s1d13521fb_info.blank_mode = blank_mode;
        return 0;
}
#endif//

//---------------------------------------------------------------------------- 
//
//---------------------------------------------------------------------------- 
#if 0
struct fb_fillrect {
        __u32 dx;       /* screen-relative */
        __u32 dy;
        __u32 width;
        __u32 height;
        __u32 color;
        __u32 rop;
};
#endif

void s1d13521fb_fillrect(struct fb_info *p, const struct fb_fillrect *rect)
{
#if 0//no use billy
	dbg_info("%s()\n",__FUNCTION__); 
        cfb_fillrect(p,rect);
       // gDisplayChange = 1;
       s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_FULL);
#endif
}

//---------------------------------------------------------------------------- 
//
//---------------------------------------------------------------------------- 
#if 0
struct fb_copyarea {
        __u32 dx;
        __u32 dy;
        __u32 width;
        __u32 height;
        __u32 sx;
        __u32 sy;
};
#endif

void s1d13521fb_copyarea(struct fb_info *p, const struct fb_copyarea *area)
{
#if 0//no use billy
	dbg_info("%s()\n",__FUNCTION__); 
        cfb_copyarea(p,area);

        //gDisplayChange = 1;
	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_FULL);
#endif
}

//---------------------------------------------------------------------------- 
//
//---------------------------------------------------------------------------- 
#if 0
struct fb_image {
        __u32 dx;               /* Where to place image */
        __u32 dy;
        __u32 width;            /* Size of image */
        __u32 height;
        __u32 fg_color;         /* Only used when a mono bitmap */
        __u32 bg_color;
        __u8  depth;            /* Depth of the image */
        const char *data;       /* Pointer to image data */
        struct fb_cmap cmap;    /* color map info */
#endif

void s1d13521fb_imageblit(struct fb_info *p, const struct fb_image *image)
{
#if 0//no use billy
//	dbg_info("%s()\n",__FUNCTION__); 
        cfb_imageblit(p, image);

//	gDisplayChange = 1;
//	s1d13521fb_set_virtual_framebuffer_cmd(EPD_IRIVER_CMD_FULL);
#endif
}

void __exit s1d13521fb_exit(void)
{
#ifdef VIDEO_REFRESH_PERIOD
	del_timer_sync(&s1d13521_timer);
#endif//VIDEO_REFRESH_PERIOD

#ifdef CONFIG_FB_EPSON_PROC
        s1d13521proc_terminate();
#endif
        unregister_framebuffer(&s1d13521_fb); 
        s1d13521if_InterfaceTerminate(&s1d13521fb_info);
}

module_init(s1d13521fb_init);
module_exit(s1d13521fb_exit);

MODULE_AUTHOR("Epson Research and Development");
MODULE_DESCRIPTION("framebuffer driver for Epson s1d13521 controller");
MODULE_LICENSE("GPL");
