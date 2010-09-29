//-----------------------------------------------------------------------------
//      
// linux/drivers/video/epson/s1d13521if.c -- frame buffer driver for Epson
// S1D13521 LCD controller.
// 
// Copyright(c) Seiko Epson Corporation 2000-2008.
// All rights reserved.
//
// This file is subject to the terms and conditions of the GNU General Public
// License. See the file COPYING in the main directory of this archive for
// more details.
//
//---------------------------------------------------------------------------- 
//---------------------------------------------------------------------------- 
//
// NOTE: This file provides all indirect interface functionality
//
//-----------------------------------------------------------------------------
#ifndef CONFIG_FB_EPSON_GPIO_S3C6410

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

//-----------------------------------------------------------------------------
//
// Local Definitions
// 
//---------------------------------------------------------------------------

typedef struct
{
        volatile u16*   cmd;
        volatile u16*   param;
} S1D13521FB_IND_STRUCT;

#define S1D_MMAP_PHYSICAL_REG_SIZE (sizeof(S1D13521FB_IND_STRUCT))

//-----------------------------------------------------------------------------
//
// Function Prototypes
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// Globals
//
//-----------------------------------------------------------------------------

static S1D13521FB_IND_STRUCT S1DIndInfo;        // indirect pointers structure.

int s1d13521if_InterfaceInit(FB_INFO_S1D13521 *info)
{
        u8 *RegAddr;
#ifdef CONFIG_FB_EPSON_PCI
        long physicalAddress;
 
        if (s1d13521pci_init(&physicalAddress) != 0)
        {
                printk(KERN_ERR "s1d13521if_InterfaceInit: PCI init failed\n"); 
                return -EINVAL;
        }
        
        RegAddr = (u8*) ioremap_nocache(physicalAddress,S1D_MMAP_PHYSICAL_REG_SIZE);
#else
        RegAddr = (u8*) ioremap_nocache(S1D_PHYSICAL_REG_ADDR,S1D_MMAP_PHYSICAL_REG_SIZE);
#endif
        if (RegAddr == NULL)
                return -EINVAL; 

        dbg_info("%s(): RegAddr %x\n", __FUNCTION__,(unsigned int)RegAddr);

        S1DIndInfo.cmd   = (u16*) (RegAddr+0);
        S1DIndInfo.param = (u16*) (RegAddr+2);

        info->RegAddr = RegAddr;
        info->RegAddrMappedSize = S1D_MMAP_PHYSICAL_REG_SIZE;
        return 0;
}

//---------------------------------------------------------------------------
// 
// 
//---------------------------------------------------------------------------
void s1d13521if_InterfaceTerminate(FB_INFO_S1D13521 *info)
{
#ifdef CONFIG_FB_EPSON_PCI
        s1d13521pci_terminate();
#endif
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
u16 s1d13521if_ReadReg16(u16 Index)
{
        u16 Value;

        lock_kernel();
        *S1DIndInfo.cmd = RD_REG;  
        *S1DIndInfo.param = Index;      // Register address

        // If HRDY is not used, we assume some host I/F minimal timings are met
        // as we cannot check for host interface busy. 
        Value = *S1DIndInfo.param;
        unlock_kernel();

        return Value;
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
void s1d13521if_BurstRead16(u16 *ptr16, unsigned copysize16)
{
        while (copysize16-- > 0)
            *ptr16++ = *S1DIndInfo.param;
}

int s1d13521if_WaitForHRDY(void)
{
#ifndef CONFIG_FB_EPSON_HRDY_OK
        {
        int cnt = HRDY_TIMEOUT_MS;

        // The host must not issue any new command until HRDY is asserted.
        // If HRDY is not used, the host can poll the Sequence Controller Busy Status
        // bit in REG[000Ah] using the RD_REG command

        // If the HRDY is not used, poll Sequence Controller Busy Status

        *S1DIndInfo.cmd = RD_REG;  
        *S1DIndInfo.param = 0x0A;       // Register address

        // Loop while host interface busy bit is set...
        while (*S1DIndInfo.param & 0x20)
        {
                if (--cnt <= 0)         // Avoid endless loop
                        break;

                mdelay(1);
        }

        if (cnt <= 0)
                {
                printk(KERN_ERR "s1d13521if_WaitForHRDY: I/F busy bit stuck\n");
                return -1;
                }

        return 0;
        }
#endif
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
void s1d13521if_WriteReg16(u16 Index, u16 Value)
{
  //      dbg_info("%s(): %02x,%02x\n",__FUNCTION__,Index,Value);

        lock_kernel();
#ifndef CONFIG_FB_EPSON_HRDY_OK
        s1d13521if_WaitForHRDY();
#endif
        *S1DIndInfo.cmd = WR_REG;  
        *S1DIndInfo.param = Index;      // Register address
        *S1DIndInfo.param = Value;      // Register value

        unlock_kernel();
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
void s1d13521if_BurstWrite16(u16 *ptr16, unsigned copysize16)
{
        while (copysize16-- > 0)
            *S1DIndInfo.param = *ptr16++;
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
int s1d13521if_cmd(unsigned ioctlcmd,s1d13521_ioctl_cmd_params *params,int numparams)
{
        int i;

        unsigned cmd = ioctlcmd & 0xFF;

        lock_kernel();

#ifndef CONFIG_FB_EPSON_HRDY_OK
        if (s1d13521if_WaitForHRDY() != 0)
        {
                unlock_kernel();
                return -1;
        }
#endif
        *S1DIndInfo.cmd = cmd;

        for (i = 0; i < numparams; i++)
                *S1DIndInfo.param = params->param[i];   

        unlock_kernel();
        return 0;
}

#endif //CONFIG_FB_EPSON_GPIO_S3C6410
