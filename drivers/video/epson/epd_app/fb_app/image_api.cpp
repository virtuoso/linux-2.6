#include <cassert>
#include <error.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include "image_api.h"


//*
#define GET565R(c)	(((c)>>11)&0x1f)
#define GET565G(c)	(((c)>>5)&0x3f)
#define GET565B(c)	((c)&0x1f)
//*/

#define GET565R_BYTE(rgb565)  ((rgb565 & 0xF800) >> 8)
#define GET565G_BYTE(rgb565)  ((rgb565 & 0x7E0) >> 3)
#define GET565B_BYTE(rgb565)  ((rgb565 & 0x1F) << 3)

#define GET_565_TO_GRAY(rgb565)	((GET565R_BYTE(rgb565) *30 + GET565G_BYTE(rgb565) *59 + GET565B_BYTE(rgb565) *11)/100)


void load_rgb565_raw_data( unsigned char *pAddr, const short * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height )
{
	int i, j, pos;	
	char* p_buffer8 = (char*) pAddr;

	pos = 0;
	for (j = 0; j < epd_height; j++)
	{
		for(i = 0; i < epd_width; i++)
		{
			if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				*(p_buffer8 + j*epd_width + i) =  GET_565_TO_GRAY(rawData[ pos ]);
				//*(p_buffer8 + j*epd_width + i) =  rgb565_LUT[ rawData[ pos ] ];
				pos++;
			}
			//*
			else
			{
				*(p_buffer8 + j*epd_width + i) = 0xf0;
			}
			//*/
		}
	}
}


void load_rgb565_raw_data_endian( unsigned char *pAddr, const short * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height )
{
	int i, j, pos;	
	char* p_buffer8 = (char*) pAddr;

	char* p_endian;
	short pixel;

	pos = 0;
	for (j = 0; j < epd_height; j++)
	{
		for(i = 0; i < epd_width; i++)
		{
			if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				p_endian = (char *)&rawData[ pos ];
				pixel = *(p_endian)<<8 |*(p_endian+1);
				*(p_buffer8 + j*epd_width + i) =  GET_565_TO_GRAY(pixel);
				pos++;
			}
			/*
			else
			{
				//*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xf0;
			}
			*/
		}
	}
}


void load_fill_data( unsigned char *pAddr, char grayData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height )
{
	int i, j, pos;	
	char* p_buffer8 = (char*) pAddr;

	pos = 0;
	for (j = img_y; j < (img_y+img_height); j++)
	{
		for(i = img_x; i < (img_x+img_width); i++)
		{
			{
				*(p_buffer8 + j*epd_width + i) =  grayData;
				pos++;
			}
		}
	}
}

#if 1
void load_gray_raw_data( unsigned char *pAddr, const char * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height )
{
	int i, j, pos;	
	char* p_buffer8 = (char*) pAddr;

	pos = 0;
	for (j = img_y; j < (img_y+img_height); j++)
	{
		for(i = img_x; i < (img_x+img_width); i++)
		{
			//if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				*(p_buffer8 + j*epd_width + i) =  rawData[ pos ];
				pos++;
			}
			/*
			else
			{
				//*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xf0;
			}
			*/
		}
	}
}
#else
void load_gray_raw_data( unsigned char *pAddr, const char * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height )
{
	int i, j, pos;	
	char* p_buffer8 = (char*) pAddr;

	pos = 0;
	for (j = 0; j < epd_height; j++)
	{
		for(i = 0; i < epd_width; i++)
		{
			if(( img_x<=i && i<(img_width+img_x) ) && ( img_y<=j && j<(img_height+img_y) ) )
			{
				*(p_buffer8 + j*epd_width + i) =  rawData[ pos ];
				pos++;
			}
			/*
			else
			{
				//*(p_buffer8 + j*S1D_DISPLAY_WIDTH + i) = 0xf0;
			}
			*/
		}
	}
}

#endif


