#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/pci.h>

#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "s1d13521ioctl.h"

#define BS_SFM_PAGE_SIZE    256

int fbfd = 0;
struct s1d13521_ioctl_hwc ioctl_hwc;

int s1d13521if_ioctl(unsigned int cmd)
{	
	if (ioctl(fbfd, cmd, &ioctl_hwc)) 
	{
		printf("IOCTL (S1D13521_REGREAD) error!!.\n");
		return 0;
	}
	
	return 1;
}


void epson_sfm_write_data( int addr, int size, char * data)
{
	int d, s;
	printf( "... write the serial flash memory (address=0x%06x, size=%d)\n", addr, size );

	ioctl_hwc.addr = addr;
	ioctl_hwc.value = 0x0000;
	s1d13521if_ioctl(S1D13521_SFM_WRITE_INIT);
	
	for ( d = 0; d < size; d++ ) 
	//for ( d = 0; d < BS_SFM_PAGE_SIZE; d++ ) 
	{
		//bs_sfm_wr_byte( data[d] );
		ioctl_hwc.addr = 0x0000;
		ioctl_hwc.value = data[d];
		s1d13521if_ioctl(S1D13521_SFM_WRITE_DATA);
	} // for d

	ioctl_hwc.addr = 0x0000;
	ioctl_hwc.value = 0x0000;
	s1d13521if_ioctl(S1D13521_SFM_WRITE_END);
}



int get_file_size(FILE *file)
{
	int begin, end;
	int cur;

	cur = ftell(file);
	//printf("cur: %d\n", cur);
	fseek(file, 0l, SEEK_SET);
	begin = ftell(file);
	//printf("begin: %d\n", begin);
	fseek(file, 0l, SEEK_END);
	end = ftell(file);
	//printf("end: %d\n", end);
	fseek(file, cur, SEEK_SET);

	return (end - begin);
}




////////////////////////////////////////////////////////
typedef enum
{
	GPA,
	GPB,
	GPC,
	GPD,
	GPE,
	GPF,
	GPG,
	GPH,
	GPI,
	GPJ,
	GPK,
	GPL,
	GPM,
	GPN,
	GPO,
	GPP,
	GPQ,
	GPNOTHING,
}GPIO_PORT;

typedef struct {
	unsigned int port_group;
	unsigned int port_num;
	unsigned int direction;		//not use
	unsigned int pull_updown;	//not use
	unsigned int value;
} egpio_status;

typedef enum
{
	OFF_STATUS = 0,
	ON_STATUS,	
}PIN_STATUS;


#define GPIODEV_NAME			"/dev/elisagpio"
#define EGPIO_IOCTL_MAGIC		288

#define EGPIO_GET				_IOR(EGPIO_IOCTL_MAGIC, 0, egpio_status)
#define EGPIO_SET				_IOW(EGPIO_IOCTL_MAGIC, 1, egpio_status)
#define EGPIO_STATUS			_IO(EGPIO_IOCTL_MAGIC, 2)


int set_gpio_value(unsigned int port_group, unsigned int port, unsigned int value)
{
	egpio_status egpio;
	egpio.port_group = port_group;
	egpio.port_num = port;
	egpio.value = value;

	int gpio_dev = -1;
	
	gpio_dev = open(GPIODEV_NAME, O_RDWR|O_NDELAY);          
       if (gpio_dev < 0)         
       {
       	printf("%s open fail in %s  !! \n", GPIODEV_NAME, __FUNCTION__);
       	return 0;
       }

   	if(ioctl(gpio_dev, EGPIO_GET, &egpio) != 0)
   	{
   		return 0;
   	}
   	
   	if(egpio.value == value)
   	{
   		//이미 동일 값 
   		return 0;
   	}
   	egpio.value = value;                     
   	ioctl(gpio_dev, EGPIO_SET, &egpio);

   	close(gpio_dev);

   	return 1;
}

////////////////////////////////////////////////////////







int main( int argc, char * argv[] )
{
//	int fbfd = 0;
//	struct s1d13521_ioctl_hwc ioctl_hwc;
	char *fbp = 0;
	unsigned char buf[16];
	unsigned int i, count = 0;

	FILE *fd_in_cmd;
	FILE *fd_in_waveform;
	char *cmd_data=0;
	char *waveform_data=0;
	int cmd_size, waveform_size, pos;

	int addr_idx, size, data_pos;
	int special_addr, gab_size;

	struct stat f_stat;

	//led on.
	set_gpio_value(GPL, 13, 1);

//read cmd
	printf("argv[1]: %s, argv[2]: %s\n", argv[1], argv[2]);
	fd_in_cmd = fopen(argv[1], "rb");
	if(fd_in_cmd == NULL)
	{
		printf("fail, file open!!\n");
		return 0;
	}

	if( stat( argv[1] , &f_stat ) != 0 )
	{
		fclose(fd_in_cmd);
		return 0;
	}
	cmd_size  = f_stat.st_size;
	printf("cmd_size: %d, (?%d)\n", cmd_size, get_file_size(fd_in_cmd));

	cmd_data = (char *)malloc(cmd_size + 1);

	pos = 0;
	count = 0;
	while ((count = fread(buf, 1, 8, fd_in_cmd)) > 0)
	{
		//printf("\n");
		for (i=0; i<count; i++)
		{
			//printf("0x%02x, ", buf[i]);
			*(cmd_data+pos) = buf[i];
			pos++;
		}
	}
//end read cmd

//read waveform
	fd_in_waveform = fopen(argv[2], "rb");
	if(fd_in_waveform == NULL)
	{
		printf("fail, file open!!\n");
		return 0;
	}

	if( stat( argv[2] , &f_stat ) != 0 )
	{
		fclose(fd_in_waveform);
		return 0;
	}
	waveform_size  = f_stat.st_size;
	printf("waveform_size: %d, (?%d)\n", waveform_size, get_file_size(fd_in_waveform));
	

	waveform_data = (char *)malloc(waveform_size + 1);

	pos = 0;
	count = 0;
	while ((count = fread(buf, 1, 8, fd_in_waveform)) > 0)
	{
		//printf("\n");
		for (i=0; i<count; i++)
		{
			//printf("0x%02x, ", buf[i]);
			*(waveform_data+pos) = buf[i];
			pos++;
		}
	}
//end read waveform

	//led off.
	set_gpio_value(GPL, 13, 0);


#if 0//data check..
{
	for(i=0; i<cmd_size; i++)
	{
		printf("0x%x ", *(cmd_data+i));

		if((i+1)%10 == 0)
		{
			printf("\n");
		}		
	}
}
#endif//data check..


#if 0//data check..
{
	for(i=0; i<waveform_size; i++)
	{
		printf("0x%x ", *(waveform_data+i));

		if((i+1)%10 == 0)
		{
			printf("\n");
		}		
	}
}
#endif//data check..


#if 1//시작을 알리는 그림을 그린다.

#endif//


#if 1//billy test..write data.
	fbfd = open("/dev/fb0", O_RDWR);
	if (!fbfd) {
		printf("Error: cannot open framebuffer device.\n");
		return -1;
	}
	printf("the framebuffer device was opend successfully.\n");


///////////////////////////////////////////////////////////
	//
	// reset
	//
	//s1d13521if_Reset();
	
	// 1. Init the clock
	//bs_init_clk();

	// 2. Init the SPI block
	//bs_init_spi();
	
	ioctl_hwc.addr = 0x0000;
	ioctl_hwc.value = 0x0000;
	s1d13521if_ioctl(S1D13521_SFM_INIT);


	//led on.
	set_gpio_value(GPL, 13, 1);
	

	// 3. Write flash file
#if 1// cmd write.
{
	//epson_sfm_bulk_erase();
	ioctl_hwc.addr = 0x0000;
	ioctl_hwc.value = 0x0000;
	s1d13521if_ioctl(S1D13521_SFM_BULK_ERASE);

	size  = cmd_size;
	count = size/BS_SFM_PAGE_SIZE;


	addr_idx = 0x000000;
	data_pos = 0;

	printf("size: %d, count: %d\n", size, count);

	//led off.
	set_gpio_value(GPL, 13, 0);
//*
	for(i=0; i<count; i++)
	{
		//led off.
		set_gpio_value(GPL, 13, 0);
		
		printf("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);
		
		epson_sfm_write_data(addr_idx, BS_SFM_PAGE_SIZE, &cmd_data[data_pos]);
		addr_idx+=BS_SFM_PAGE_SIZE;
		data_pos+=BS_SFM_PAGE_SIZE;

		//led on.
		set_gpio_value(GPL, 13, 1);
	}

	if(size > (count*BS_SFM_PAGE_SIZE))
	{
		printf("addr_idx: 0x%x, data_pos: %d, size-data_pos: %d\n", addr_idx, data_pos, size-data_pos);
		epson_sfm_write_data(addr_idx, size-data_pos, &cmd_data[data_pos]);
	}
//*/
}
#endif//

#if 1// waveform write.
{
	size = waveform_size;
	count = size/BS_SFM_PAGE_SIZE;

	addr_idx = 0x000886;
	data_pos = 0;

	printf("size: %d, count: %d\n", size, count);

	//0x900까지는 별도로 쓴다.
	printf("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);

	special_addr = 0x000900;
	gab_size = special_addr - addr_idx;
	epson_sfm_write_data(addr_idx, gab_size, &waveform_data[data_pos]);
	addr_idx+=gab_size;
	data_pos+=gab_size;

	size = waveform_size-gab_size;
	count = size/BS_SFM_PAGE_SIZE;

	for(i=0; i<count; i++)
	{
		//led off.
		set_gpio_value(GPL, 13, 0);
		
		printf("addr_idx: 0x%x, data_pos: %d\n", addr_idx, data_pos);
		
		epson_sfm_write_data(addr_idx, BS_SFM_PAGE_SIZE, &waveform_data[data_pos]);
		addr_idx+=BS_SFM_PAGE_SIZE;
		data_pos+=BS_SFM_PAGE_SIZE;

		//led on.
		set_gpio_value(GPL, 13, 1);
	}

	if(size > (count*BS_SFM_PAGE_SIZE))
	{
		printf("addr_idx: 0x%x, data_pos: %d, size-data_pos: %d\n", addr_idx, data_pos, size-data_pos);
		printf("==> addr_idx: 0x%x, data_pos: %d, waveform_size-data_pos: %d\n", addr_idx, data_pos, waveform_size-data_pos);

		//epson_sfm_write_data(addr_idx, size-data_pos, &waveform_data[data_pos]);
		epson_sfm_write_data(addr_idx, waveform_size-data_pos, &waveform_data[data_pos]);
	}
}
#endif//
///////////////////////////////////////////////////////////

	//led off.
	set_gpio_value(GPL, 13, 0);

	sleep(2);

	//led on.
	set_gpio_value(GPL, 13, 1);
	
	printf("finish write waveform date!\nwaiting epd reboot!!\n");
	ioctl_hwc.addr = 0x0000;
	ioctl_hwc.value = 0x0000;
	s1d13521if_ioctl(S1D13521_EPD_IRIVER_REBOOT);
	sleep(1);

	//led off.
	set_gpio_value(GPL, 13, 0);
#endif//test...


	printf("epd reboot ok!!!\n");

	free(cmd_data);
	free(waveform_data);

	fclose(fd_in_cmd);
	fclose(fd_in_waveform);
	close(fbfd);

	//led on.
	set_gpio_value(GPL, 13, 1);
	
	return 0;
}


