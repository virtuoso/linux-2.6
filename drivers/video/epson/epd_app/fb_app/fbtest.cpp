#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include "s1d13521ioctl.h"

//#include "pgm.h"
#include "image_api.h"

#include "time.h"

extern char binary[];//rgb565
extern char k_binary_48x48[];//8gray
extern char a_binary_48x48[];//8gray
extern char b_binary_48x48[];//8gray
extern char bg1_100x100[];
extern char bg2_100x100[];

static int *_pEPD_cmd = 0;


/*
1~4 byte: cmd
5~8 byte: x position
9~12byte: y position
13~16byte: width
17~20byte: height

21~32byte; reserved.
*/

//항상 우선적으로 실행되어야한다.
void epd_init_cmd(int *pEPD_data_end)
{
	printf("epd_init_cmd(). pEPD_data_end: 0x%x\n", pEPD_data_end);
	_pEPD_cmd = (int *)(pEPD_data_end+1);
	printf("epd_init_cmd(). _pEPD_cmd: 0x%x\n", _pEPD_cmd);
}

void epd_set_cmd(int cmd)
{
	*_pEPD_cmd = cmd;
}

void epd_set_area(int x, int y, int width, int height)
{
	*(_pEPD_cmd+1) = x;
	*(_pEPD_cmd+2) = y;
	*(_pEPD_cmd+3) = width;
	*(_pEPD_cmd+4) = height;
}

int epd_get_cmd(void)
{
	return *_pEPD_cmd;
}

void epd_wait_display(void)
{
	int cmd = *_pEPD_cmd;
	
	while ( ( cmd != EPD_IRIVER_CMD_INIT ) )
	{
		cmd = *_pEPD_cmd;
		//printf("=cmd: 0x%x=\n", cmd);
	}
}



int main( int argc, char * argv[] )
{
	int fbfd = 0;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	struct s1d13521_ioctl_hwc ioctl_hwc;
		
	long int screensize = 0;
	char *fbp = 0;
	unsigned char *pEPD = 0;
	int x = 0, y = 0;
	long int location = 0;


	fbfd = open("/dev/fb0", O_RDWR);
	if (!fbfd) {
		printf("Error: cannot open framebuffer device.\n");
		return -1;
	}
	printf("the framebuffer device was opend successfully.\n");

	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error #1 reading fixed information.\n");
		return -1;
	}

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error #2 reading variable information.\n");
		return -1;
	}

	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel/8 + 32;

	printf("vinfo.xres=== %d\n",vinfo.xres);
	printf("vinfo.yres=== %d\n",vinfo.yres);
	printf("vinfo.bits_per_pixel/8==== %d\n",vinfo.bits_per_pixel/8);

	fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
						fbfd, 0);
	if ((int)fbp == -1) {
		printf("Error: failed to map framebuffer device to memory.\n");
		return -1;
	}

	printf("The framebuffer device was mapped to memory successfully. fbp: 0x%x\n", fbp);
	printf("x = %d, y = %d\n", vinfo.xoffset, vinfo.yoffset);

	pEPD = (unsigned char *)fbp;

	/*꼭해야함.*/
	epd_init_cmd( (int *)(pEPD + vinfo.xres*vinfo.yres));


	#if 0// for using ioctl.
	ioctl_hwc.value = EPD_IRIVER_CMD_FULL;
	if (ioctl(fbfd, S1D13521_EPD_IRIVER_CMD, &ioctl_hwc)) 
	{
		printf("IOCTL (S1D13521_REGREAD) error!!.\n");
		return 0;
	}
	#endif//0



// rotation사용 방법.
#if 0
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}

	printf("draw white!!.\n");
	epd_set_cmd(EPD_IRIVER_CMD_FULL);
	
	//0도
	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 10, 10, 500, 510, vinfo.yres, vinfo.xres );//500x510
	epd_set_cmd(EPD_ROTATION_0);
	printf("> rotation 0.\n");

	//90도
	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 10, 10, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_cmd(EPD_ROTATION_90);
	printf("> rotation 90.\n");

	//180도
	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 10, 10, 500, 510, vinfo.yres, vinfo.xres );
	epd_set_cmd(EPD_ROTATION_180);
	printf("> rotation 180.\n");

	//270도
	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 10, 10, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_cmd(EPD_ROTATION_270);
	printf("> rotation 270.\n");

	//90도
	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 10, 10, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_cmd(EPD_ROTATION_90);
	printf("> rotation 90.\n");
#endif
//end rotation test!!!


// part area update.
#if 0
	sleep(2);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 30, 100, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(30, 100, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	printf("> EPD_IRIVER_CMD_PART_AREA. #1\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 30, 150, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(30, 150, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	printf("> EPD_IRIVER_CMD_PART_AREA. #2\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 40, 200, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(40, 200, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	printf("> EPD_IRIVER_CMD_PART_AREA. #3\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 50, 250, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(50, 250, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	printf("> EPD_IRIVER_CMD_PART_AREA.#4\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 60, 300, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(60, 300, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	printf("> EPD_IRIVER_CMD_PART_AREA. #5\n");
#endif
// end part area update.


// part area update.
#if 0
	sleep(2);
	epd_wait_display();
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}
	epd_set_cmd(EPD_IRIVER_CMD_FULL);

	sleep(1);
	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 30, 100, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(30, 100, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_FULL_AREA);
	printf("> EPD_IRIVER_CMD_FULL_AREA. #1\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 30, 150, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(30, 150, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_FULL_AREA);
	printf("> EPD_IRIVER_CMD_FULL_AREA. #2\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 40, 200, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(40, 200, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_FULL_AREA);
	printf("> EPD_IRIVER_CMD_FULL_AREA. #3\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 50, 250, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(50, 250, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_FULL_AREA);
	printf("> EPD_IRIVER_CMD_FULL_AREA.#4\n");

	epd_wait_display();
	load_rgb565_raw_data( pEPD, (short *) binary , 60, 300, 500, 510, vinfo.xres, vinfo.yres );
	epd_set_area(60, 300, 500, 510);
	epd_set_cmd(EPD_IRIVER_CMD_FULL_AREA);
	printf("> EPD_IRIVER_CMD_FULL_AREA. #5\n");
#endif//
// end part area update.

// part area update test 2.
#if 0
	sleep(2);
	epd_wait_display();
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}
	epd_set_cmd(EPD_IRIVER_CMD_FULL);

	sleep(1);
	
	for (y = 0; y < 16; y++) 
	{
		for (x = 0; x < 12; x++) 
		{
			printf("a > x: %d, y: %d\n", x, y);
			epd_wait_display();
			load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
			epd_set_area(48*x, 48*y, 48, 48);
			epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);

			if(y>2)
			{
				epd_wait_display();
				load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*(y-3), 48, 48, vinfo.xres, vinfo.yres );
				epd_set_area(48*x, 48*(y-3), 48, 48);
				epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);

				epd_wait_display();
				load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*(y-2), 48, 48, vinfo.xres, vinfo.yres );
				epd_set_area(48*x, 48*(y-2), 48, 48);
				epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
			}
		}
	}
#endif
// end part area update.


// part area update test 3.
#if 0
{
	int count;
	clock_t start, end;
	

	epd_wait_display();

printf("image copy start! #1\n");
	start = clock();
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}
	end = clock();
printf("image copy end!time: %d\n", (end - start) );

printf("image copy start! #2\n");
	start = clock();
	for (y = 0; y < 16; y++) 
	{
		for (x = 0; x < 12; x++) 
		{
			load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		}
	}
	end = clock();
//printf("image copy end!time: %d\n", (end - start) / CLOCKS_PER_SEC);
printf("image copy end!time: %d\n", (end - start) );

	epd_set_cmd(EPD_IRIVER_CMD_FULL);

	sleep(1);

for(count=0; count<100000; count++)
{
	y = 0;
	for (x = 0; x < 12; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 11;
	for (y = 0; y < 16; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 15;
	for (x = 11; x > -1; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 0;
	for (y = 15; y > -1; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

/////////////////////////////////////////

	y = 0;
	for (x = 0; x < 12; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, b_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 11;
	for (y = 0; y < 16; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, b_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 15;
	for (x = 11; x > -1; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, b_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 0;
	for (y = 15; y > -1; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, b_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

}
//////////////////////////////////////

	y = 0;
	for (x = 0; x < 12; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 11;
	for (y = 0; y < 16; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 15;
	for (x = 11; x > -1; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 0;
	for (y = 15; y > -1; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
		epd_set_area(48*x, 48*y, 48, 48);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

/*	
	for (y = 0; y < 16; y++) 
	{
		for (x = 0; x < 12; x++) 
		{
			printf("a > x: %d, y: %d\n", x, y);
			epd_wait_display();
			load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*y, 48, 48, vinfo.xres, vinfo.yres );
			epd_set_area(48*x, 48*y, 48, 48);
			epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);

			if(y>2)
			{
				epd_wait_display();
				load_gray_raw_data( pEPD, k_binary_48x48 , 48*x, 48*(y-3), 48, 48, vinfo.xres, vinfo.yres );
				epd_set_area(48*x, 48*(y-3), 48, 48);
				epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);

				epd_wait_display();
				load_gray_raw_data( pEPD, a_binary_48x48 , 48*x, 48*(y-2), 48, 48, vinfo.xres, vinfo.yres );
				epd_set_area(48*x, 48*(y-2), 48, 48);
				epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
			}
		}
	}
*/
}
#endif
// end part area update.


//update test 4
#if 0
{
	int count;
	clock_t start, end;
	

	epd_wait_display();

printf("image copy start! #1\n");
	start = clock();
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}
	end = clock();
printf("image copy end!time: %d\n", (end - start) );

printf("image copy start! #2\n");
	start = clock();
	for (y = 0; y < 8; y++) 
	{
		for (x = 0; x < 6; x++) 
		{
			load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		}
	}
	end = clock();
//printf("image copy end!time: %d\n", (end - start) / CLOCKS_PER_SEC);
printf("image copy end!time: %d\n", (end - start) );

	epd_set_cmd(EPD_IRIVER_CMD_FULL);

	sleep(1);

for(count=0; count<100000; count++)
{
	y = 0;
	for (x = 0; x < 6; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 5;
	for (y = 0; y < 8; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 7;
	for (x = 5; x > -1; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 0;
	for (y = 7; y > -1; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}
//////////////////////////

	y = 1;
	for (x = 1; x < 5; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 4;
	for (y = 1; y < 7; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 6;
	for (x = 4; x > 0; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 1;
	for (y = 6; y > 0; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg2_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

/////////////////////////////////////////

	y = 0;
	for (x = 0; x < 6; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 5;
	for (y = 0; y < 8; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 7;
	for (x = 5; x > -1; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 0;
	for (y = 7; y > -1; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 1;
	for (x = 1; x < 5; x++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 4;
	for (y = 1; y < 7; y++) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	y = 6;
	for (x = 4; x > 0; x--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}

	x = 1;
	for (y = 6; y > 0; y--) 
	{
		epd_wait_display();
		load_gray_raw_data( pEPD, bg1_100x100 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA);
	}
}
//////////////////////////////////////
}
#endif

//update test 5
#if 1
{
	int count;
	clock_t start, end;
	

	epd_wait_display();

printf("image copy start! #1\n");
	start = clock();
	for (y = 0; y < vinfo.yres; y++) 
	{
		for (x = 0; x < vinfo.xres; x++) 
		{
			*(pEPD + x + y*vinfo.xres) = 0xf0;
		}
	}
	end = clock();
printf("image copy end!time: %d\n", (end - start) );

printf("600x800 copy start! #2\n");
	start = clock();
	load_fill_data( pEPD, 0xff , 0, 0, 600, 800, vinfo.xres, vinfo.yres );
	end = clock();
//printf("image copy end!time: %d\n", (end - start) / CLOCKS_PER_SEC);
printf("600x800 copy end!time: %d\n", (end - start) );

	//epd_set_area(0, 0, 600, 800);
	epd_set_cmd(EPD_IRIVER_CMD_PART_BW);

	sleep(1);

for(count=0; count<10; count++)
{
	y = 0;
	for (x = 0; x < 6; x++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 5;
	for (y = 0; y < 8; y++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	y = 7;
	for (x = 5; x > -1; x--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 0;
	for (y = 7; y > -1; y--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}
//////////////////////////

	y = 1;
	for (x = 1; x < 5; x++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 4;
	for (y = 1; y < 7; y++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	y = 6;
	for (x = 4; x > 0; x--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 1;
	for (y = 6; y > 0; y--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0x00 , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

/////////////////////////////////////////

	y = 0;
	for (x = 0; x < 6; x++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 5;
	for (y = 0; y < 8; y++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	y = 7;
	for (x = 5; x > -1; x--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 0;
	for (y = 7; y > -1; y--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	y = 1;
	for (x = 1; x < 5; x++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 4;
	for (y = 1; y < 7; y++) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	y = 6;
	for (x = 4; x > 0; x--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}

	x = 1;
	for (y = 6; y > 0; y--) 
	{
		epd_wait_display();
		load_fill_data( pEPD, 0xff , 100*x, 100*y, 100, 100, vinfo.xres, vinfo.yres );
		epd_set_area(100*x, 100*y, 100, 100);
		epd_set_cmd(EPD_IRIVER_CMD_PART_AREA_BW);
	}
}
//////////////////////////////////////
}
#endif


	munmap(fbp, screensize);
	close(fbfd);
	return 0;
}


