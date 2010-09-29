//-----------------------------------------------------------------------------
//      
// linux/drivers/video/epson/s1d13521ifgpio.c -- Gumstix specific GPIO 
// interface code for Epson S1D13521 LCD controllerframe buffer driver.
// 
// Code based on E-Ink demo source code. 
//
// Copyright(c) Seiko Epson Corporation 2008.
// All rights reserved.
//
// This file is subject to the terms and conditions of the GNU General Public
// License. See the file COPYING in the main directory of this archive for
// more details.
//
//---------------------------------------------------------------------------- 

#ifdef CONFIG_FB_EPSON_GPIO_S3C6410
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "s1d13521fb.h"

//billy add
//#include <asm/arch/gpio.h>
//#include <asm/arch/regs-gpio.h>
//#include <asm/arch/regs-lcd.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-lcd.h>


//---------------------------------------------------------------------------
//  
// Local Definitions
//
//---------------------------------------------------------------------------


#define GPIO_CNFX       16
#define GPIO_CNF1       17

#ifdef CONFIG_FB_EPSON_HRDY_OK
#define GPIO_HRDY       32
#endif

#define GPIO_HDC        48 
#define GPIO_RESET_L    49
#define GPIO_HRD_L      74
#define GPIO_HWE_L      75
#define GPIO_HCS_L      76
#define GPIO_HIRQ       77
#define GPIO_HDB        58 // 58-73
#define MAP_SIZE        4096

// gpio registers

#define REG(r) (*(volatile unsigned int *)((char*)_map+(r)))

#if 1//6410
#define GPIO_ADDR       (0x77100000)
#else
#define GPIO_ADDR       0x40E00000
#define GPIO_LR         0x0000
#define GPIO_DR         0x000C
#define GPIO_SR         0x0018
#define GPIO_CR         0x0024
#define GPIO_RER        0x0030
#define GPIO_FER        0x003C
#define GPIO_EDR        0x0048
#define GPIO_AFR        0x0054  //GPIO Alternate Function Register
#endif//6410

//---------------------------------------------------------------------------
//  
// Local Globals
// 
//---------------------------------------------------------------------------
//static void * _map;

//---------------------------------------------------------------------------
//
// Local Function Prototypes  
//
//---------------------------------------------------------------------------
void init_gpio(void);
//static void set_gpio_val(int pin, int val);
//static void set_gpio_mode(int pin);
//static void set_gpio_dir(int pin, int val);
//static int  get_gpio_val(int pin);
static void command(int v);
static void data(int v);
static int  data_get(void);
static int  gpio_hdb_get(void);
//static void gpio_hdb(int val);
static void set_cmd_mode(void);

extern int get_elice_board_version(void);

#if defined(CONFIG_FB_EPSON_DEBUG_PRINTK) || defined (CONFIG_FB_EPSON_PROC)
//static int get_gpio_mode(int pin);
//static int get_gpio_dir(int pin);
#endif

#ifdef CONFIG_FB_EPSON_HRDY_OK
static int  wait_for_ready(void);
#endif

//woong
int epson_epd_reboot(int gray_val);

static unsigned long temp;


void s1d13521if_extern_ready_Reset(void)
{
	dbg_info("%s()\n", __FUNCTION__);
	//gpio_direction_output(S3C_GPL12, 1);
	//gpio_set_value(S3C_GPL12, 1);

	s3c_gpio_cfgpin(S3C64XX_GPL(12), S3C_GPIO_OUTPUT);//output pin
	gpio_set_value(S3C64XX_GPL(12), 1);

}
EXPORT_SYMBOL_GPL(s1d13521if_extern_ready_Reset);



void s1d13521if_Reset(void)
{
	dbg_info("%s()\n", __FUNCTION__);
	s3c_gpio_cfgpin(S3C64XX_GPL(12), S3C_GPIO_OUTPUT);//output pin
	mdelay(80);

	gpio_set_value(S3C64XX_GPL(12), 0);
	mdelay(100);

	gpio_set_value(S3C64XX_GPL(12), 1);
	mdelay(30);
}



//
// write data in Serial Flash for instruction code and waveform.
//
#include "s1d13521binary.h"

void bs_sfm_wait_for_bit( int ra, int pos, int val )
{
	int v = s1d13521if_ReadReg16( ra );
	while ( ( ( v >> pos ) & 0x1 ) != ( val & 0x1 ) )
	{
		v = s1d13521if_ReadReg16( ra );
		//woong
		udelay(10);//billy check. 090701
	}
}


//extern unsigned char Cmd0047c0fi250fo5d0200s00cd8d02_bin[];	//instruction code.
//extern unsigned char V110_Q011_60_VE0102_BTC_wbf[];			// waveform date.

#define BS_SFM_WREN 0x06	//write enable
#define BS_SFM_WRDI 0x04	//write disable
#define BS_SFM_RDSR 0x05	//read staus register
#define BS_SFM_READ 0x03	//read data bytes
#define BS_SFM_PP   0x02		//page program
#define BS_SFM_SE   0xD8		//sector erase
#define BS_SFM_BE   0xC7		//bulk erase
#define BS_SFM_RES  0xAB
#define BS_SFM_ESIG 0x11

#define BS_SFM_SECTOR_COUNT 4 // total size=> 256kbyte(262144bytes)
#define BS_SFM_SECTOR_SIZE  (64*1024)//65536 byte. , 256 page.
#define BS_SFM_PAGE_SIZE    256
#define BS_SFM_PAGE_COUNT   (BS_SFM_SECTOR_SIZE/BS_SFM_PAGE_SIZE)//256

void bs_sfm_wr_byte( int data )
{
	int v = ( data & 0xFF ) | 0x100;
	s1d13521if_WriteReg16( 0x202, v );
	bs_sfm_wait_for_bit( 0x206, 3, 0 );
}

void bs_sfm_write_enable( void )
{
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_WREN );
	s1d13521if_WriteReg16( 0x0208, 0 );
}

int bs_sfm_rd_byte( void )
{
	int v;
	s1d13521if_WriteReg16( 0x202, 0 );
	bs_sfm_wait_for_bit( 0x206, 3, 0 );
	v = s1d13521if_ReadReg16( 0x200 );
	return ( v & 0xFF );
}

int bs_sfm_read_status( void )
{
	int s;
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_RDSR );
	s = bs_sfm_rd_byte( );
	s1d13521if_WriteReg16( 0x0208, 0 );
	return s;
}


#if 0
void epson_sfm_write_disable( void )
{
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_WRDI );
	s1d13521if_WriteReg16( 0x0208, 0 );
}



void epson_sfm_program_page( int pa, int size, char * data )
{
	int d, s;
	bs_sfm_write_enable( );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_PP );
	bs_sfm_wr_byte( ( pa >> 16 ) & 0xFF );
	bs_sfm_wr_byte( ( pa >> 8 ) & 0xFF );
	bs_sfm_wr_byte( pa & 0xFF );
	for ( d = 0; d < BS_SFM_PAGE_SIZE; d++ ) 
	{
		bs_sfm_wr_byte( data[d] );
	} // for d
	s1d13521if_WriteReg16( 0x0208, 0 );
	while ( true ) 
	{
		s = bs_sfm_read_status( );
		if ( ( s & 0x1 ) == 0 ) break;
	} // while
}


void epson_sfm_program_sector( int sa, int size, char * data )
{
	int pa, p, y;
	dbg_info( "... programming sector (0x%06x)\n", sa );
	pa = sa;
	for ( p = 0; p < BS_SFM_PAGE_COUNT; p++ ) 
	{
		dbg_info("[%d/%d]\n", p, BS_SFM_PAGE_COUNT);
		y = p * BS_SFM_PAGE_SIZE;
		epson_sfm_program_page( pa, BS_SFM_PAGE_SIZE, &data[y] );
		pa += BS_SFM_PAGE_SIZE;
	} // for p
}
#endif


#if 1//billy make..
void epson_sfm_write_data( int addr, int size, char * data )
{
	int d, s;
	dbg_info( "... write the serial flash memory (address=0x%06x, size=%d)\n", addr, size );
	bs_sfm_write_enable( );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_PP );
	bs_sfm_wr_byte( ( addr >> 16 ) & 0xFF );
	bs_sfm_wr_byte( ( addr >> 8 ) & 0xFF );
	bs_sfm_wr_byte( addr & 0xFF );
	for ( d = 0; d < size; d++ ) 
	//for ( d = 0; d < BS_SFM_PAGE_SIZE; d++ ) 
	{
//		if(d < size)
//		{
			//dbg_info("[%d/%d, 0x%x]: 0x%x\n", d, size, addr+d, data[d]);
			bs_sfm_wr_byte( data[d] );
//		}
//		else
//		{
//			dbg_info("+ [%d/%d, 0x%x]: 0x%x\n", d, size, addr+d, 0xff);
			//bs_sfm_wr_byte( 0x00 );
//		}
	} // for d

	s1d13521if_WriteReg16( 0x0208, 0 );
	while ( true ) 
	{
		s = bs_sfm_read_status( );
		//dbg_info("===bs_sfm_read_status(): 0x%x\n", s);
		if ( ( s & 0x1 ) == 0 ) break;
	} // while
}
#endif//billy make.

//for appication.
void epson_ioctl_sfm_write_init( int addr)
{
	bs_sfm_write_enable( );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_PP );
	bs_sfm_wr_byte( ( addr >> 16 ) & 0xFF );
	bs_sfm_wr_byte( ( addr >> 8 ) & 0xFF );
	bs_sfm_wr_byte( addr & 0xFF );
}

void epson_ioctl_sfm_write_end(void)
{
	int s;

	s1d13521if_WriteReg16( 0x0208, 0 );
	while ( true ) 
	{
		s = bs_sfm_read_status( );
		//dbg_info("===bs_sfm_read_status(): 0x%x\n", s);
		if ( ( s & 0x1 ) == 0 ) break;
	} // while
}
//end for appication

void epson_sfm_erase( int addr )
{
	int s;
	dbg_info( "... erasing sector (0x%06x)\n", addr );
	bs_sfm_write_enable( );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_SE );
	bs_sfm_wr_byte( ( addr >> 16 ) & 0xFF );
	bs_sfm_wr_byte( ( addr >> 8 ) & 0xFF );
	bs_sfm_wr_byte( addr & 0xFF );
	s1d13521if_WriteReg16( 0x0208, 0 );
	while ( true ) 
	{
		s = bs_sfm_read_status( );
		if ( ( s & 0x1 ) == 0 ) break;
	} // while
}


void epson_sfm_bulk_erase(void)
{
	int s;
	dbg_info( "... bulk erasing\n");
	bs_sfm_write_enable( );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_BE );
//	bs_sfm_wr_byte( ( addr >> 16 ) & 0xFF );
//	bs_sfm_wr_byte( ( addr >> 8 ) & 0xFF );
//	bs_sfm_wr_byte( addr & 0xFF );
	s1d13521if_WriteReg16( 0x0208, 0 );
	while ( true ) 
	{
		s = bs_sfm_read_status( );
		if ( ( s & 0x1 ) == 0 ) break;
	} // while
}


void epson_sfm_read( int addr, int size, char * data )
{
	int i;
	dbg_info( "... reading the serial flash memory (address=0x%06x, size=%d)\n", addr, size );
	s1d13521if_WriteReg16( 0x0208, 1 );
	bs_sfm_wr_byte( BS_SFM_READ );
	bs_sfm_wr_byte( ( addr >> 16 ) & 0xFF );
	bs_sfm_wr_byte( ( addr >> 8 ) & 0xFF );
	bs_sfm_wr_byte( addr & 0xFF );
	for ( i = 0; i < size; i++ ) 
	{
		data[i] = bs_sfm_rd_byte( );
		dbg_info("[%d/%d, 0x%x] :0x%x\n", i, size, addr+i, data[i]);
	}
	s1d13521if_WriteReg16( 0x0208, 0 );
	dbg_info( "... reading the serial flash memory --- done\n" );
}

#if 0//no use
//epson_sfm_write(0x04, sizeof(Cmd0047c0fi250fo5d0200s00cd8d02_bin), Cmd0047c0fi250fo5d0200s00cd8d02_bin);
void epson_sfm_write( int addr, int size, char * data )
{
	int s1, s2;
	int x, s;
	int i;
	int sa, start, count, limit;
	char *sd, *rd;
	
	dbg_info( "... writing the serial flash memory (address=0x%06x, size=%d)\n", addr, size );

	//char * sd = new char [ BS_SFM_SECTOR_SIZE ];
	sd = kmalloc(BS_SFM_SECTOR_SIZE, GFP_KERNEL);
	
	if ( sd == NULL )
	{
		dbg_info("[%s] !!! ERROR: no memory for sd\n", __FUNCTION__ );
		return;
	}

	s1 = addr / BS_SFM_SECTOR_SIZE;
	s2 = ( addr + size - 1 ) / BS_SFM_SECTOR_SIZE;

	dbg_info("s1: 0x%x, s2: 0x%x, addr: 0x%x\n", s1, s2, addr);

	x = 0;
	for (s = s1; s <= s2; s++ ) 
	{
		sa = s * BS_SFM_SECTOR_SIZE;
		start = 0;
		count = BS_SFM_SECTOR_SIZE;
		if ( s == s1 ) 
		{
			dbg_info("# 1, s: 0x%x, s1: 0x%x\n", s, s1);
			if ( addr > sa ) 
			{
				start = addr - sa;
				epson_sfm_read( sa, start, sd );
			}
		}
		if ( s == s2 ) 
		{
			limit = addr + size;
			dbg_info("# 2, limit: 0x%x, sa: 0x%x\n", limit, sa);
			if ( ( sa + BS_SFM_SECTOR_SIZE ) > limit ) 
			{
				count = limit - sa;
				#if 0//billy
				epson_sfm_read( limit, ( sa + BS_SFM_SECTOR_SIZE - limit ), &sd[count] );
				#endif//0
			}
		}
		epson_sfm_erase( sa );
		for ( i = start; i < count; i++ ) 
		{
			assert( x < size );
			sd[i] = data[x++];
		}
		epson_sfm_program_sector( sa, BS_SFM_SECTOR_SIZE, sd );
	} // for s

	epson_sfm_write_disable( );

	//delete [] sd;
	kfree(sd);

	//char * rd = new char [ size ];
	rd = kmalloc(size, GFP_KERNEL);
	assert( rd != NULL );

	dbg_info( "... verifying the serial flash memory write\n" );

	epson_sfm_read( addr, size, rd );

	for ( i = 0; i < size; i++ ) 
	{
		if ( rd[i] != data[i] ) 
		{
			dbg_info( "+++++++++++++++ rd[%d]=0x%02x  data[%d]=0x%02x\n", i, rd[i], i, data[i] );
			dbg_info( "[%s] !!! ERROR: failed to verify the flash memory write data", __FUNCTION__ );
		}
	} // for i

	//delete [] rd;
	kfree(rd);

	dbg_info( "... writing the serial flash memory --- done\n" );
}
#endif//0


void bs_init_clk(void)
{
	bool PLL_Not_Locked;

	dbg_info("\n%s():: ++!\n", __FUNCTION__);

/*billy add code*/
	s1d13521if_WriteReg16( 0x016, 0x0003 );//PLL Power Down Enable.(not be changed while the PLL is active, REG[0016h] bit 1=0b)
	s1d13521if_WriteReg16( 0x006, 0x0000 );// power save mode disabled.

	
	// 1. Configure PLL
	//s1d13521if_WriteReg16(0x0010, 0x4); //
	//s1d13521if_WriteReg16(0x0012, 0x5<<12 | 0x9<<8 | 0x09<<3 | 0x1);
	//s1d13521if_WriteReg16(0x0014, 0x4<<4); // 4+1 X 25 Mhz == 125Mhz
	s1d13521if_WriteReg16(0x0010, 0x3); //
assert( s1d13521if_ReadReg16( 0x0010 ) == 0x3 );

	s1d13521if_WriteReg16(0x0012, 0x5949);
assert( s1d13521if_ReadReg16( 0x0012 ) == 0x5949 );

	s1d13521if_WriteReg16(0x0014, 0x40); // 4+1 X 25 Mhz == 125Mhz
assert( s1d13521if_ReadReg16( 0x0014 ) == 0x40 );

	// 2. Disable PLL power down
	s1d13521if_WriteReg16(0x0016, 0x0001); // Disable PLL power down

	// 3. Wait for PLL lock
	PLL_Not_Locked = true;
	while (PLL_Not_Locked)
	{
		if ( (s1d13521if_ReadReg16(0x0A) & 0x0001) == 0x0001)
		{
			PLL_Not_Locked = false;
		}
		else
		{
			dbg_info("<bs_init_clk> .... Waiting for PLL Lock\n");
		}
	}
	dbg_info("<bs_init_clk> .... PLL LOCKED\n");

	// 4. Disable Power Save and pll bypass
	s1d13521if_WriteReg16(0x0016, 0x0000); //pll bypass disable, System Clock Divide Select 2:1
	s1d13521if_WriteReg16(0x0006, 0x0000); //power save disable

	dbg_info("%s():: --!\n", __FUNCTION__);
}


void bs_init_spi(void) 
{
	dbg_info("\n%s():: ++!\n", __FUNCTION__);

	//initialize SPI
	s1d13521if_WriteReg16(0x0204, 0x0039);//org 9:1
	//s1d13521if_WriteReg16(0x0204, 0x0001);// 2:1 fail.
	//s1d13521if_WriteReg16(0x0204, 0x0011);// 4:1 one more try.
	//s1d13521if_WriteReg16(0x0204, 0x0031);// 8:1

	s1d13521if_WriteReg16(0x0208, 0x00);

	dbg_info("%s():: --!\n", __FUNCTION__);
}


#if 0//no use
void start_spi( void )
{
	s1d13521if_WriteReg16(0x0208, 0x01);
}

void end_spi( void )
{
	s1d13521if_WriteReg16(0x0208, 0x00);
}


void write_spi_byte( int d )
{
	s1d13521if_WriteReg16(0x0202, (0x0100|(d&0xFF)));
	while( !(s1d13521if_ReadReg16(0x0206) & 0x08) ){ dbg_info("busy..\n"); }
}


void bs_sfm_write( int addr, int size, char * data )
{
	int addr_idx,size_idx, i;
	dbg_info("\n<Write wfm data from SPI Flash Memory Interface>\n");
	
	// Split up into 256byte Page chunks
	addr_idx = addr;
	for (size_idx = 0; size_idx < size; size_idx = size_idx + 256)
	{
		// 1. Set Chip select
		start_spi();
		
		// 2. Send Write Command
		write_spi_byte(0x02); // Page Program

		// 3. Send Write addresses
		write_spi_byte((addr_idx>>16)& 0xFF); // address1
		write_spi_byte((addr_idx>>8 )& 0xFF); // address2
		write_spi_byte((addr_idx )& 0xFF); // address3

		// 4. Send Write SPI
		if ((size_idx + 256) > size)
		{
			for( i=size_idx ; i<size ; i++ )
			{
				//write_spi_byte(data++);
				write_spi_byte(*data++);
			}
		}
		else
		{
			for( i=0 ; i<256 ; i++ )
			{
				write_spi_byte(*data++);
			}
		}
	
		//5. Deselect Chip Select
		end_spi();
	}//for
}
#endif//0

#ifdef CONFIG_FB_EPSON_SFM_ERASE
void s1d13521if_serial_flash_erase(void)
{
	static int init = 1;

	if(!init)
	{
		return;
	}
	dbg_info("\n%s():: start!\n", __FUNCTION__);

	//
	// reset
	//
	s1d13521if_Reset();

	// 1. Init the clock
	bs_init_clk();

	// 2. Init the SPI block
	bs_init_spi();

	// 3. Write flash file
	epson_sfm_bulk_erase();

	init = 0;

	mdelay(200);
}
#endif//CONFIG_FB_EPSON_SFM_ERASE

#ifdef CONFIG_FB_EPSON_WRITE_BINARY
void s1d13521if_SetCodeInSerialFlash(void)
{
	dbg_info("\n%s():: start!\n", __FUNCTION__);

	//
	// reset
	//
	s1d13521if_Reset();

	// 1. Init the clock
	bs_init_clk();

	// 2. Init the SPI block
	bs_init_spi();

	// 3. Write flash file
#if 1// cmd write.
{
	int addr_idx, i, size, count, data_pos;

	epson_sfm_bulk_erase();

	size = sizeof(Cmd0047c0fi250fo5d0200s00cd8d02_bin);
	count = size/BS_SFM_PAGE_SIZE;

	addr_idx = 0x000000;
	data_pos = 0;

	dbg_info("size: %d, count: %d\n", size, count);

	for(i=0; i<count; i++)
	{
		dbg_info("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);
		
		epson_sfm_write_data(addr_idx, BS_SFM_PAGE_SIZE, &Cmd0047c0fi250fo5d0200s00cd8d02_bin[data_pos]);
		addr_idx+=BS_SFM_PAGE_SIZE;
		data_pos+=BS_SFM_PAGE_SIZE;
	}

	if(size > (count*BS_SFM_PAGE_SIZE))
	{
		dbg_info("addr_idx: 0x%x, data_pos: %d, size-data_pos: %d\n", addr_idx, data_pos, size-data_pos);
		epson_sfm_write_data(addr_idx, size-data_pos, &Cmd0047c0fi250fo5d0200s00cd8d02_bin[data_pos]);
	}
}
#endif//

#if 1// waveform write.
{
	int addr_idx, i, size, count, data_pos;
	int special_addr, gab_size;

	size = sizeof(V110_Q011_60_VE0102_BTC_wbf);
	count = size/BS_SFM_PAGE_SIZE;

	addr_idx = 0x000886;
	data_pos = 0;

	dbg_info("size: %d, count: %d\n", size, count);

	//0x900까지는 별도로 쓴다.
	dbg_info("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);

	special_addr = 0x000900;
	gab_size = special_addr - addr_idx;
	epson_sfm_write_data(addr_idx, gab_size, &V110_Q011_60_VE0102_BTC_wbf[data_pos]);
	addr_idx+=gab_size;
	data_pos+=gab_size;

	count = (size-gab_size)/BS_SFM_PAGE_SIZE;
		

	for(i=0; i<count; i++)
	{
		dbg_info("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);
		
		epson_sfm_write_data(addr_idx, BS_SFM_PAGE_SIZE, &V110_Q011_60_VE0102_BTC_wbf[data_pos]);
		addr_idx+=BS_SFM_PAGE_SIZE;
		data_pos+=BS_SFM_PAGE_SIZE;
	}

	if((size-gab_size) > (count*BS_SFM_PAGE_SIZE))
	{
		dbg_info("addr_idx: 0x%x, data_pos: %d, size-data_pos: %d\n", addr_idx, data_pos, size-data_pos);
		epson_sfm_write_data(addr_idx, size-data_pos, &V110_Q011_60_VE0102_BTC_wbf[data_pos]);
	}
}
#endif//

#if 0//cmd verify..
{
	int i, size;
	char * rd;

	size = sizeof(Cmd0047c0fi250fo5d0200s00cd8d02_bin);
	rd = kmalloc(size, GFP_KERNEL);
	assert( rd != NULL );

	dbg_info( "... verifying the serial flash memory write[cmd]\n" );
	
	epson_sfm_read( 0x000000, size, rd );

	for ( i = 0; i < size; i++ ) 
	{
		if ( rd[i] != Cmd0047c0fi250fo5d0200s00cd8d02_bin[i] ) 
		{
			dbg_info( "ERROR: failed to verify, rd[%d]=0x%02x  data[%d]=0x%02x\n", i, rd[i], i, Cmd0047c0fi250fo5d0200s00cd8d02_bin[i] );
		}
	} // for i

	kfree(rd);

	mdelay(3000);
}
#endif//0

#if 0//waveform verify..
{
	int i, size;
	char * rd;

	size = sizeof(V110_Q011_60_VE0102_BTC_wbf);
	rd = kmalloc(size, GFP_KERNEL);
	assert( rd != NULL );

	dbg_info( "... verifying the serial flash memory write[waveform]\n" );
	
	epson_sfm_read( 0x000886, size, rd );

	for ( i = 0; i < size; i++ ) 
	{
		if ( rd[i] != V110_Q011_60_VE0102_BTC_wbf[i] ) 
		{
			dbg_info( "ERROR: failed to verify, rd[%d]=0x%02x  data[%d]=0x%02x\n", i, rd[i], i, V110_Q011_60_VE0102_BTC_wbf[i] );
		}
	} // for i

	kfree(rd);

	mdelay(3000);
}
#endif//0


#if 0 //RES test.
	s1d13521if_WriteReg16( 0x208, 0x0001 );//cs enable

	bs_sfm_wr_byte(0xab);
	bs_sfm_wr_byte(0x00);
	bs_sfm_wr_byte(0x00);
	bs_sfm_wr_byte(0x00);
	dbg_info("0x%x()??? 0x11\n", bs_sfm_rd_byte());

	s1d13521if_WriteReg16( 0x208, 0x0000 );//cs disable.
#endif//0

#if 0 //RDID test.
	s1d13521if_WriteReg16( 0x208, 0x0001 );//cs enable

	bs_sfm_wr_byte(0x9f);
	dbg_info("0x%x manufacturer id\n", bs_sfm_rd_byte());
	dbg_info("0x%x device id #1\n", bs_sfm_rd_byte());
	dbg_info("0x%x device id #2\n", bs_sfm_rd_byte());

	s1d13521if_WriteReg16( 0x208, 0x0000 );//cs disable.
#endif//0


	dbg_info("\n%s():: end read test!\n", __FUNCTION__);

	mdelay(200);
}

#endif//CONFIG_FB_EPSON_WRITE_BINARY




//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
int s1d13521if_InterfaceInit(FB_INFO_S1D13521 *info)
{
/*// billy no use.
        u8* RegAddr;
        RegAddr  = (unsigned char*) ioremap_nocache(GPIO_ADDR,MAP_SIZE);

        if (RegAddr == NULL)
        {               
                printk(KERN_ERR "s1d13521if_InterfaceInit (Gumstix): ioremap_nocache failed\n"); 
                return -EINVAL; 
        }

        dbg_info("s1d13521ifgpio: %s():: RegAddr %x\n", __FUNCTION__,(unsigned int)RegAddr);

        info->RegAddr = RegAddr;
        info->RegAddrMappedSize = MAP_SIZE;
        _map = (void*)info->RegAddr;
*/
        init_gpio();

#ifdef CONFIG_FB_EPSON_SFM_ERASE
	s1d13521if_serial_flash_erase();
#endif//CONFIG_FB_EPSON_SFM_ERASE

#ifdef CONFIG_FB_EPSON_WRITE_BINARY
	s1d13521if_SetCodeInSerialFlash();
#endif//CONFIG_FB_EPSON_WRITE_BINARY
	
       set_cmd_mode(); // command mode, reset the chip 
	return 0;
}

//---------------------------------------------------------------------------
// 
//---------------------------------------------------------------------------
void s1d13521if_InterfaceTerminate(FB_INFO_S1D13521 *info)
{
}

//---------------------------------------------------------------------------
// 
//---------------------------------------------------------------------------
int s1d13521if_WaitForHRDY(void)
{
#ifndef CONFIG_FB_EPSON_HRDY_OK

	int cnt = HRDY_TIMEOUT_MS;

	// The host must not issue any new command until HRDY is asserted.
	// If HRDY is not used, the host can poll the Sequence Controller Busy Status
	// bit in REG[000Ah] using the RD_REG command

	// If the HRDY is not used, poll Sequence Controller Busy Status
	//set_gpio_val(GPIO_HCS_L, 0);
        //set_gpio_val(GPIO_HCS_L, 0);
        temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	
	command(RD_REG);
        //set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> high

	data(0x0A);                     // Register address

	// Loop while host interface busy bit is set...
	while (data_get() & 0x20)
	{
	        if (--cnt <= 0)         // Avoid endless loop
	                break;

	        mdelay(1);
	}

	//        set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high

	if (cnt <= 0)
	{
	    printk(KERN_ERR "s1d13521if_WaitForHRDY: I/F busy bit stuck\n");
	    return -1;
	}

	return 0;

#else
        return wait_for_ready();
#endif
}


void s1d13521if_lock_kernel(void)
{
	lock_kernel();
}

void s1d13521if_unlock_kernel(void)
{
	unlock_kernel();
}

//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
u16 s1d13521if_ReadReg16(u16 Index)
{
	u16 Value;

	s1d13521if_lock_kernel();

#if 0
	// HDC, SYS_RS 에 해당. 
	// 첫번째 커맨드일때  HDC를 Low로 만듬.
	temp = readl(S3C_SIFCCON0);
	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> low

	mdelay(2);//billy add
	

	//CS는 커맨드가 끝날때까지 low로 유지한다.
	//set_gpio_val(GPIO_HCS_L, 0);
	// safe code...
	temp = readl(S3C_SIFCCON0);
	mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	// end safe code...
	
	mdelay(2);
	command(WR_REG); //manual mode enable, CS0(low)상태에서 HRD(high), HWE(low)  ==> data write ==> HWE(high), manual mode disable.
	mdelay(2);
	
	//set_gpio_val(GPIO_HDC, 1); 
	// safe code...
	temp = readl(S3C_SIFCCON0);
	mdelay(2);//billy add
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high
	// end safe code...

	data(Index);                    // Register address
#else
	// 첫번째 커맨드일때  HDC를 Low로 만듬.
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> low
	udelay(1);
	
        //set_gpio_val(GPIO_HCS_L, 0);
        temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	udelay(2);
	
        command(RD_REG);
        //set_gpio_val(GPIO_HDC, 1);
        temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> high

	udelay(1);
	data(Index);	// Register address
#endif
        Value = data_get();

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
	s1d13521if_unlock_kernel();
	
	//dbg_info("s1d13521ifgpio: %s(): Reg[%02xh]=%02xh\n",__FUNCTION__, Index,Value);
        return Value;
}

//---------------------------------------------------------------------------
//  
// 
//---------------------------------------------------------------------------
void s1d13521if_WriteReg16(u16 Index, u16 Value)
{
	//dbg_info("s1d13521ifgpio: %s(): 0x%02x,0x%02x\n",__FUNCTION__,Index,Value);
	s1d13521if_WaitForHRDY();

	s1d13521if_lock_kernel();

	// HDC, SYS_RS 에 해당. 
	// 첫번째 커맨드일때  HDC를 Low로 만듬.
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> low

	//mdelay(2);//billy add
	

	//CS는 커맨드가 끝날때까지 low로 유지한다.
	//set_gpio_val(GPIO_HCS_L, 0);
	// safe code...
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	// end safe code...
	
	//mdelay(2);
	command(WR_REG); //manual mode enable, CS0(low)상태에서 HRD(high), HWE(low)  ==> data write ==> HWE(high), manual mode disable.
	udelay(2);//billy add
	
	//set_gpio_val(GPIO_HDC, 1); 
	// safe code...
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high
	// end safe code...

	data(Index);                    // Index:  Register address
	data(Value);                    // Register value

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
	s1d13521if_unlock_kernel();
}

//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
int s1d13521if_cmd(unsigned ioctlcmd,s1d13521_ioctl_cmd_params *params,int numparams)
{
       int i;
       unsigned cmd = ioctlcmd & 0xFF;
	
//	dbg_info("s1d13521ifgpio: %s()++: cmd=%xh numparams=%d\n",__FUNCTION__, cmd,numparams);

	if (s1d13521if_WaitForHRDY() != 0)
	{
	    //s1d13521if_unlock_kernel();
	    return -1;
	}
	
	s1d13521if_lock_kernel();
	
	// 첫번째 커맨드일때  HDC를 Low로 만듬.
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> low

//       set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//mdelay(2);//billy add
	command(cmd);
	//mdelay(2);//billy add

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high

	for (i = 0; i < numparams; i++)
	{
		//dbg_info("[%d]:0x%x\n", i, params->param[i]);
		data(params->param[i]);
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high

	s1d13521if_unlock_kernel();
	 
	return 0;
}


int s1d13521if_WaitForHRDY_in_reg(int wait_time);

//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
int s1d13521if_reg_cmd(unsigned ioctlcmd,s1d13521_ioctl_cmd_params *params,int numparams)
{
       int i;
       unsigned cmd = ioctlcmd & 0xFF;
	
//	dbg_info("s1d13521ifgpio: %s()++: cmd=%xh numparams=%d\n",__FUNCTION__, cmd,numparams);

	
	
	if (s1d13521if_WaitForHRDY_in_reg(50) != 0)
	{
		//s1d13521if_unlock_kernel();
		return -1;
	}

	s1d13521if_lock_kernel();
	// 첫번째 커맨드일때  HDC를 Low로 만듬.
	temp = readl(S3C_SIFCCON0);
//	mdelay(2);//billy add
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> low

//       set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//mdelay(2);//billy add
	command(cmd);
	//mdelay(2);//billy add

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high

	for (i = 0; i < numparams; i++)
	{
		//dbg_info("[%d]:0x%x\n", i, params->param[i]);
		data(params->param[i]);
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high

	s1d13521if_unlock_kernel();
	 
	return 0;
}


//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
void s1d13521if_BurstWrite16_Area(u8 *ptr8, int x, int y, int width, int height)
{
	//woong
#ifdef CONFIG_FB_EPSON_ENABLE_4BPP
	int i;
	unsigned char *data = (char*)ptr16;

	dbg_info("burst write 4bpp\n");
	
	//set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high

	i =0;
	while (copysize16-=2 > 0)
	{
		i+=2;
		writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
		writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

		writel((u16)( (((int)(data[i]&0xF0))>>4)
		| ((int)(data[i+1]&0xF0))
		| (((int)(data[i+2]&0xF0))<<4)
		| (((int)(data[i+3]&0xF0))<<8) ), S3C_SIFCCON1);// Put data
		
		writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
		writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

		//udelay(1);//billy check. 090701
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
	
#else
	int i, j;
	unsigned char *data = ptr8 + (y*S1D_DISPLAY_WIDTH + x);
	unsigned short send_data16[2], b_fill;
	unsigned char *ptr_data8 = (unsigned char *)send_data16;

	//set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high


	b_fill = 0;
	for (j = 0; j < height; j++) 
	{
		for (i = 0; i < width; i++) 
		{
			if(b_fill == 0)
			{
				*(ptr_data8) = *(data + (j*S1D_DISPLAY_WIDTH + i));
				b_fill++;
			}
			else if(b_fill == 1)
			{
				*(ptr_data8+1) = *(data + (j*S1D_DISPLAY_WIDTH + i));
				b_fill++;
			}

			if(b_fill == 2)//send data.
			{
				writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
				writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

				writel(send_data16[0], S3C_SIFCCON1);// Put data

				writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
				writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

				b_fill=0;
			}

			//udelay(1);//billy check. 090701
			//*(d_gray + (y*_vinfo.xres + x)) = 0x00;
		}
	}

	if(b_fill == 1)//send data.
	{
		*(ptr_data8+1) = 0xff;
		
		writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
		writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

		writel(send_data16[0], S3C_SIFCCON1);// Put data

		writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
		writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

		b_fill=0;
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
#endif//CONFIG_FB_EPSON_ENABLE_4BPP
}



void s1d13521if_BurstWrite16(u16 *ptr16, unsigned copysize16)
{
#ifdef CONFIG_FB_EPSON_ENABLE_4BPP
	int i;
	unsigned char *data = (char*)ptr16;

	dbg_info("burst write 4bpp\n");
	
	//set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high

	i =0;
	while (copysize16-=2 > 0)
	{
		i+=2;
		writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
		writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

		writel((u16)( (((int)(data[i]&0xF0))>>4)
		| ((int)(data[i+1]&0xF0))
		| (((int)(data[i+2]&0xF0))<<4)
		| (((int)(data[i+3]&0xF0))<<8) ), S3C_SIFCCON1);// Put data
		
		writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
		writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

		//udelay(1);//billy check. 090701
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
	
#else
	//set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x20, S3C_SIFCCON0);//HDC -> high

	while (copysize16-- > 0)
	{
		writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
		writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

		writel(*ptr16++, S3C_SIFCCON1);// Put data
		
		writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
		writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

		//udelay(1);//billy check. 090701
	}

	//set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high
#endif//CONFIG_FB_EPSON_ENABLE_4BPP
}

//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
void s1d13521if_BurstRead16(u16 *ptr16, unsigned copysize16)
{	
	dbg_info("s1d13521ifgpio: %s()++: start ptr16=%xh copysize16=%d\n",__FUNCTION__, *ptr16,copysize16);
	
	//	set_gpio_val(GPIO_HCS_L, 0);
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low

	//	set_gpio_val(GPIO_HDC, 1);
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> high

	while (copysize16-- > 0)
	{
	#if 1
		*ptr16++ = data_get();
	#else
		//set_gpio_val(GPIO_HRD_L, 0); 
		temp = readl(S3C_SIFCCON0);
		writel(temp | 0x04, S3C_SIFCCON0);//HRD -> low

		*ptr16++ = gpio_hdb_get();

		//set_gpio_val(GPIO_HRD_L, 1); 
		temp = readl(S3C_SIFCCON0);
		writel(temp & ~0x04, S3C_SIFCCON0);//HRD -> high
	#endif//1
	}

	    //set_gpio_val(GPIO_HCS_L, 1);
	temp = readl(S3C_SIFCCON0);
	//mdelay(2);//billy add
	writel(temp & ~0x10, S3C_SIFCCON0);//HCS -> high

	dbg_info("s1d13521ifgpio: %s()--: last ptr16=%xh copysize16=%d\n",__FUNCTION__, *ptr16--,copysize16);
}



extern void s1d13521fb_goto_normal(void);
void epson_epd_power_off(void)
{
	//일반포트 disable시킨다.
	s3c_gpio_cfgpin(S3C64XX_GPI(0), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(0), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(1), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(2), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(2), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(3), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(3), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(4), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(4), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(5), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(5), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(6), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(7), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(7), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(8), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(8), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(9), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(9), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(10), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(10), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(11), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(11), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(12), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(12), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(13), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(13), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(14), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(14), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPI(15), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPI(15), S3C_GPIO_PULL_NONE);
	
	s3c_gpio_cfgpin(S3C64XX_GPJ(0), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(0), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(1), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(2), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(2), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(3), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(3), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(4), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(4), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(5), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(5), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(6), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(7), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(7), S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(S3C64XX_GPJ(8), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(8), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(9), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(9), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(10), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(10), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C64XX_GPJ(11), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S3C64XX_GPJ(11), S3C_GPIO_PULL_NONE);

	writel(0x0, S3C64XX_GPICONSLP);
	writel(0x0, S3C64XX_GPIPUDSLP);
	
	writel(0x0, S3C64XX_GPJCONSLP);
	writel(0x0, S3C64XX_GPJPUDSLP);

	//video control.
	writel(0x0, S3C_VIDCON0);
	writel(0x0, S3C_VIDCON1);

	//i80 interface control
	writel(0x0, S3C_I80IFCONA0);
	writel(0x0, S3C_I80IFCONA1);
	writel(0x0, S3C_I80IFCONB0);
	writel(0x0, S3C_I80IFCONB1);

	//special port control
	writel(0x0000010, S3C64XX_SPC_BASE);

	//modem interface port control.(마지막에 해야함... 위쪽핀들이 컨트롤이 안될거 같음.)
	writel(0x00000008, S3C_HOSTIFB_MIFPCON);

	// i80/RGB trigger control
	writel(0x00, S3C_TRIGCON);

	//i80 system interface manual command control
	writel(0x00, S3C_SIFCCON0);
}
EXPORT_SYMBOL_GPL(epson_epd_power_off);



//---------------------------------------------------------------------------
//  
//---------------------------------------------------------------------------
void init_gpio(void)
{
	int val;
	   
	dbg_info("s1d13521ifgpio: %s():\n",__FUNCTION__);

	/* Must be '0' for Normal-path instead of By-pass */
	writel(0x0, S3C_HOSTIFB_MIFPCON);

	/*Must be set as '00' to use Host I/F*/
	/* //org
	val = readl(S3C_SPCON);
	val &= ~0x3;
	writel(val, S3C_SPCON);
	*/
	val = readl(S3C64XX_SPC_BASE);
	val &= ~0x3;
	writel(val, S3C64XX_SPC_BASE);
	
	/* VD */
#if 0 //org
	gpio_set_pin(S3C_GPI0, S3C_GPI0_LCD_VD0);
	gpio_set_pin(S3C_GPI1, S3C_GPI1_LCD_VD1);
	gpio_set_pin(S3C_GPI2, S3C_GPI2_LCD_VD2);
	gpio_set_pin(S3C_GPI3, S3C_GPI3_LCD_VD3);
	gpio_set_pin(S3C_GPI4, S3C_GPI4_LCD_VD4);
	gpio_set_pin(S3C_GPI5, S3C_GPI5_LCD_VD5);
	gpio_set_pin(S3C_GPI6, S3C_GPI6_LCD_VD6);
	gpio_set_pin(S3C_GPI7, S3C_GPI7_LCD_VD7);
	gpio_set_pin(S3C_GPI8, S3C_GPI8_LCD_VD8);
	gpio_set_pin(S3C_GPI9, S3C_GPI9_LCD_VD9);
	gpio_set_pin(S3C_GPI10, S3C_GPI10_LCD_VD10);
	gpio_set_pin(S3C_GPI11, S3C_GPI11_LCD_VD11);
	gpio_set_pin(S3C_GPI12, S3C_GPI12_LCD_VD12);
	gpio_set_pin(S3C_GPI13, S3C_GPI13_LCD_VD13);
	gpio_set_pin(S3C_GPI14, S3C_GPI14_LCD_VD14);
	gpio_set_pin(S3C_GPI15, S3C_GPI15_LCD_VD15);
	
	gpio_set_pin(S3C_GPJ0, S3C_GPJ0_LCD_VD16);
	gpio_set_pin(S3C_GPJ1, S3C_GPJ1_LCD_VD17);
	gpio_set_pin(S3C_GPJ2, S3C_GPJ2_LCD_VD18);
	gpio_set_pin(S3C_GPJ3, S3C_GPJ3_LCD_VD19);
	gpio_set_pin(S3C_GPJ4, S3C_GPJ4_LCD_VD20);
	gpio_set_pin(S3C_GPJ5, S3C_GPJ5_LCD_VD21);
	gpio_set_pin(S3C_GPJ6, S3C_GPJ6_LCD_VD22);
	gpio_set_pin(S3C_GPJ7, S3C_GPJ7_LCD_VD23);

	
	gpio_set_pin(S3C_GPJ8, S3C_GPJ8_LCD_HSYNC);
	gpio_set_pin(S3C_GPJ9, S3C_GPJ9_LCD_VSYNC);
	gpio_set_pin(S3C_GPJ10, S3C_GPJ10_LCD_VDEN);
	gpio_set_pin(S3C_GPJ11, S3C_GPJ11_LCD_VCLK);
#else
	s3c_gpio_cfgpin(S3C64XX_GPI(0), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(1), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(2), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(3), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(4), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(5), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(6), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(7), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(8), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(9), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(10), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(11), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(12), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(13), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(14), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPI(15), S3C_GPIO_SFN(2));
	
	s3c_gpio_cfgpin(S3C64XX_GPJ(0), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(1), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(2), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(3), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(4), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(5), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(6), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(7), S3C_GPIO_SFN(2));

	s3c_gpio_cfgpin(S3C64XX_GPJ(8), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(9), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(10), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(S3C64XX_GPJ(11), S3C_GPIO_SFN(2));
#endif//

	
	//I80 enable.

	/*LCD_SetClkSrc, ref samsung code.*/
	temp=readl(S3C_VIDCON0);
	temp &= ~(0x3<<2);
	temp |= (0x0<<2);
	writel(temp, S3C_VIDCON0);
	/*LCD_SetClkSrc*/

	/*LCD_SetClkDirOrDiv, ref samsung code.*/
	temp=readl(S3C_VIDCON0);
	temp &= ~(0x1<<4);
	writel(temp, S3C_VIDCON0);
	/*LCD_SetClkDirOrDiv*/

/*
	temp =
		LCD_CS_SETUP((u32)0) |
		LCD_WR_SETUP((u32)1) |
		LCD_WR_ACT((u32)4) |
		LCD_WR_HOLD((u32)0) |
		RSPOL_HIGH | SUCCEUP_TRIGGERED | SYSIFEN_ENABLE;	//Check the Bit1
*/
 	temp =  ((((u32)0)&0xF)<<16) | ((((u32)1)&0xF)<<12) | ((((u32)4)&0xF)<<8) | ((((u32)0)&0xF)<<4) |
			(1<<2) | (1<<1) | (1<<0);	//Check the Bit1
	writel(temp, S3C_I80IFCONA0);

	mdelay(10);

	//enable trigger.
	writel(0x03, S3C_TRIGCON);

	mdelay(10);
	//Manual command mode.
	writel(0x01, S3C_SIFCCON0);

#ifdef CONFIG_FB_EPSON_HRDY_OK

#if 0//billy no use..
	if(get_elice_board_version() == ELICE_BOARD_EVM1)//only evm
	{
		temp = readl(S3C64XX_GPP_BASE);
		writel(temp & ~0x30, S3C64XX_GPP_BASE);//GPP2 -> input pin.
		printk("EPD GPIO EVM, HRDY: %d\n", readl(S3C64XX_GPP_BASE)&0x04);
	}
	else
#endif//no use...
	{
		//s3c_gpio_cfgpin(S3C_GPL14, S3C_GPL14_INP);
		//s3c_gpio_pullup(S3C_GPL14, 1);
		//printk("check GPIO, HRDY: %d\n", (s3c_gpio_getpin(S3C_GPL14)?1:0));

		s3c_gpio_cfgpin(S3C64XX_GPL(14), S3C_GPIO_INPUT);
		s3c_gpio_setpull(S3C64XX_GPL(14), S3C_GPIO_PULL_UP);
		printk("check GPIO, HRDY: %d\n", (gpio_get_value(S3C64XX_GPL(14))?1:0));
	}
#endif//CONFIG_FB_EPSON_HRDY_OK

}



//---------------------------------------------------------------------------
//CS0 => low상태에서 HRD=>high, HWE=>low
//---------------------------------------------------------------------------
void command(int v)
{
/*
	assert(get_gpio_val(GPIO_HCS_L) == 0);
	set_gpio_val(GPIO_HDC, 0);
	set_gpio_val(GPIO_HWE_L, 0); 
	gpio_hdb(v);
	set_gpio_val(GPIO_HWE_L, 1); 
*/
	temp = 0x10;//CS -> low

//	udelay(2);
	writel(temp|0x01, S3C_SIFCCON0); //Manual Command Mode Enable.
	writel(temp|0x03, S3C_SIFCCON0); //HRD = low, HWE = Low, Manual Command Mode

	udelay(2);//billy add
	
	writel(v, S3C_SIFCCON1);// Put data

	udelay(2);//billy add
	
	writel(temp|0x01, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
	writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

}

//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
void data(int v)
{	
//	dbg_info("s1d13521ifgpio: %s(): %02xh\n",__FUNCTION__, (unsigned)v);
	
//	assert(get_gpio_val(GPIO_HCS_L) == 0);
//	assert(get_gpio_val(GPIO_HDC) == 1);
/*
	temp = readl(S3C_SIFCCON0);
	//writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	printk("HCS -> low: (1) ? %d\n", temp & 0x10);
	temp = readl(S3C_SIFCCON0);
	//writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> high
	printk("HCS -> high: (0) ? %d\n", temp & 0x20);
*/	


//	set_gpio_val(GPIO_HWE_L, 0); 
//	gpio_hdb(v);
//	set_gpio_val(GPIO_HWE_L, 1); 

	temp = 0x10;//CS -> low

//	mdelay(2);//billy add
#if 1
	writel(temp|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
//	mdelay(2);//billy add
	writel(temp|0x23, S3C_SIFCCON0); //HRD->Hign, HWE->Low, Manual Command Mode Enable

//	mdelay(2);//billy add
	udelay(2);

	writel(v, S3C_SIFCCON1);// Put data
	udelay(2);
	
	writel(temp|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
//	mdelay(2);//billy add
	writel(temp|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.
	udelay(1);
#else	
	writel(temp|0xe1, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
	mdelay(2);//billy add
	writel(temp|0xe3, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = Low, Manual Command Mode

	mdelay(2);//billy add

	writel(v, S3C_SIFCCON1);// Put data

	mdelay(2);//billy add
	
	writel(temp|0xe1, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
	mdelay(2);//billy add
	writel(temp|0x00, S3C_SIFCCON0); //CS =High
#endif
//	mdelay(2);//billy add

}

//---------------------------------------------------------------------------
// The data bus direction is already set to input
//---------------------------------------------------------------------------
int data_get(void)
{
        int d;

//        assert(get_gpio_val(GPIO_HCS_L) == 0);
//        assert(get_gpio_val(GPIO_HDC) == 1);
/*
	temp = readl(S3C_SIFCCON0);
	//writel(temp | 0x10, S3C_SIFCCON0);//HCS -> low
	printk("HCS -> low: (1) ? %d\n", temp & 0x10);
	temp = readl(S3C_SIFCCON0);
	//writel(temp & ~0x20, S3C_SIFCCON0);//HDC -> high
	printk("HCS -> high: (0) ? %d\n", temp & 0x20);
*/


//        set_gpio_val(GPIO_HRD_L, 0); 
	temp = readl(S3C_SIFCCON0);
	writel(temp | 0x04, S3C_SIFCCON0);//HRD -> low

	udelay(1);
	
	d = gpio_hdb_get();

//	mdelay(2);//billy add
		
//        set_gpio_val(GPIO_HRD_L, 1); 
	temp = readl(S3C_SIFCCON0);
	writel(temp & ~0x04, S3C_SIFCCON0);//HRD -> high

//	printk("data_get: %d\n", d);
        return d;
}

//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#ifdef CONFIG_FB_EPSON_HRDY_OK
int hrdy_fail_cnt = 0;
int wait_for_ready(void)
{
        int cnt = HRDY_TIMEOUT_MS;

#if 0//billy no use... evm
	if(get_elice_board_version() == ELICE_BOARD_EVM1)//only evm
	{
		int d = readl(S3C_GPPDAT)&0x04;

		while (d == 0) 
		{
		        mdelay(1);

		        if (--cnt <= 0)         // Avoid endless loop
		        {       
		                printk(KERN_ERR "s1d13521if_cmd: wait_for_ready: I/F busy bit stuck\n");
		                return -1;
		        }

		        //d = get_gpio_val(GPIO_HRDY);
		        d = readl(S3C_GPPDAT)&0x04;
		} 
	}
	else
#endif//no use.
	{
		//int d = (s3c_gpio_getpin(S3C_GPL14)?1:0);
		int d = (gpio_get_value(S3C64XX_GPL(14))?1:0);

		while (d == 0) 
		{
		        msleep(1);
				//printk(KERN_ERR "s1d13521if_cmd: wait_for_ready: %d\n", hrdy_fail_cnt);

		        if (--cnt <= 0)         // Avoid endless loop
		        {       
				printk(KERN_ERR "s1d13521if_cmd: wait_for_ready: I/F busy bit stuck, hrdy_fail_cnt: %d\n", hrdy_fail_cnt);
				hrdy_fail_cnt++;
				return -1;
		        }

		        //d = get_gpio_val(GPIO_HRDY);
		        d = (gpio_get_value(S3C64XX_GPL(14))?1:0);

			//add billy.
			if(d == 0 && hrdy_fail_cnt>3)
			{
				printk(KERN_ERR "s1d13521if_cmd: wait_for_ready: I/F busy bit stuck, maybe waveform fail????\n");
				return -2;
			}
		} 
	}

	return 0;
}


int s1d13521if_WaitForHRDY_in_reg(int wait_time)
{
        int cnt = wait_time;//ms

#if 0//no use.. billy
	if(get_elice_board_version() == ELICE_BOARD_EVM1)//only evm
	{
		int d = readl(S3C_GPPDAT)&0x04;

		while (d == 0) 
		{
		        mdelay(1);

		        if (--cnt <= 0)         // Avoid endless loop
		        {       
		                printk(KERN_ERR "s1d13521if_WaitForHRDY_in_reg: wait_for_ready: I/F busy bit stuck\n");
		                return -1;
		        }

		        //d = get_gpio_val(GPIO_HRDY);
		        d = readl(S3C_GPPDAT)&0x04;
		} 
	}
	else
#endif//no use..
	{
	        int d = (gpio_get_value(S3C64XX_GPL(14))?1:0);

	        while (d == 0) 
	        {
	                msleep(1);

	                if (--cnt <= 0)         // Avoid endless loop
	                {       
	                        printk(KERN_ERR "s1d13521if_WaitForHRDY_in_reg: wait_for_ready: I/F busy bit stuck\n");
	                        return -1;
	                }

	                //d = get_gpio_val(GPIO_HRDY);
	                d = (gpio_get_value(S3C64XX_GPL(14))?1:0);
	        } 
	}

	return 0;
}
#endif

/*
//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0
void set_gpio_val(int pin, int val)
{
        int r = (pin >> 5) << 2;
        int b = pin & 0x1F;
        unsigned int v = 1 << b;

        if (val & 0x1) 
                REG(GPIO_SR + r) = v;
        else
                REG(GPIO_CR + r) = v;
}
#endif//0
*/



/*
//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0
void gpio_hdb(int val)
{
#if 1//samsung
	u32 uMode;
	uMode = 0x10;//main...
	
	writel(uMode|0x01, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
	writel(uMode|0x03, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = Low, Manual Command Mode

	writel(val, S3C_SIFCCON1);// Put data
	
	writel(uMode|0x01, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
	writel(uMode|0x00, S3C_SIFCCON0); //CS =High
	
#else
        unsigned int v;

        v = val & 0x3F;

        if (v != 0) 
                REG(GPIO_SR + 4) = v << 26;

        v = (~v) & 0x3F;

        if (v != 0) 
                REG(GPIO_CR + 4) = v << 26;

        v = (val >> 6) & 0x3FF;

        if (v != 0) 
                REG(GPIO_SR + 8) = v;

        v = (~v) & 0x3FF;

        if (v != 0) 
                REG(GPIO_CR + 8) = v;
#endif
}
#endif

*/

//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
int gpio_hdb_get(void)
{
#if 1
	int d;
	#if 1//samsung
	u32 uMode;
	uMode = 0x10;//main...

//	mdelay(2);//billy add
	writel(uMode|0x21, S3C_SIFCCON0); //HDC->High, Manual Command Mode Enable.
//	mdelay(1);//billy add
	writel(uMode|0x25, S3C_SIFCCON0); //HRD->Hign, HRD->Low, Manual Command Mode Enable

	mdelay(1);//billy add
	d = readl(S3C_SIFCCON2);// get data.
	mdelay(1);//billy add
	
	writel(uMode|0x21, S3C_SIFCCON0); //RS = Low, nOE = High, nWE = High, Manual Command Mode
//	mdelay(1);//billy add
	writel(uMode|0x00, S3C_SIFCCON0); //Manual Command Mode -> normal mode.

	#else
	d = readl(S3C_SIFCCON2);
	#endif
#else
        unsigned int v;
        int d;

        v = REG(GPIO_LR + 4);
        d = (v >> 26) & 0x3F;
        v = REG(GPIO_LR + 8);
        d |= (v & 0x3FF) << 6;
#endif
        return d;
}


/*
//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0
void set_gpio_dir(int pin, int val)
{
        int b, r;

        r = (pin >> 5) << 2;
        r += GPIO_DR;
        b = pin & 0x1F;

        if (val & 0x1) 
                REG(r) |= (1 << b);
        else
                REG(r) &= ~(1 << b);
}
#endif
*/

/*
//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0//no use
void set_gpio_mode(int pin)
{
        int r,b;

//      dbg_info("s1d13521ifgpio: %s(): %d\n",__FUNCTION__,pin);

        r = (pin >> 4) << 2;
        b = (pin & 0xF) << 1;

#ifdef CONFIG_FB_EPSON_DEBUG_PRINTK
        if (pin == 49) 
                assert((r == 12) && (b == 2));

        if (pin == 75) 
                assert((r == 16) && (b == 22));
#endif 
        r += GPIO_AFR;
        REG(r) &= ~(3 << b);
}
#endif//

*/

/*
#if defined(CONFIG_FB_EPSON_DEBUG_PRINTK) || defined (CONFIG_FB_EPSON_PROC)
//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0//no use
int get_gpio_mode(int pin)
{
        int r,b,v;

        r = (pin >> 4) << 2;
        r += GPIO_AFR;
        b = (pin & 0xF) << 1;
        v = REG(r);
        return (v >> b) & 0x3;
}
#endif//0

//---------------------------------------------------------------------------
//
//---------------------------------------------------------------------------
#if 0// no use.
int get_gpio_dir(int pin)
{
        int r,b,v;

        r = (pin >> 5) << 2;
        r += GPIO_DR;
        b = pin & 0x1F;
        v = REG(r);
        return (v >> b) & 0x1;
}
#endif//0
#endif
*/


/*
#if 0
int get_gpio_val(int pin)
{
        int r,b;
        unsigned int v;

        r = (pin >> 5) << 2;
        b = pin & 0x1F;
        v = REG(GPIO_LR + r);
        return (v >> b) & 0x1;
}
#endif
*/

void set_cmd_mode(void)
{
        //dbg_info("s1d13521ifgpio: %s():\n",__FUNCTION__);

// no use
 //       set_gpio_val(GPIO_CNF1, 1);

	s1d13521if_Reset();

#ifdef CONFIG_FB_EPSON_HRDY_OK
        printk("s1d13521ifgpio: %s():\n",__FUNCTION__);
        wait_for_ready();
#endif
}

#ifdef CONFIG_FB_EPSON_PROC
typedef struct
{
        const int gpio;
        const char *gpiostr;
}GPIONAMEST;

#endif //CONFIG_FB_EPSON_PROC

#endif // CONFIG_FB_EPSON_GPIO_S3C6410

