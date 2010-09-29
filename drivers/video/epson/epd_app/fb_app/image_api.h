//#include "pgm.h"

//extern void load_pgm_file( const char * fn , unsigned char *pAddr, int img_x, int img_y, int epd_width, int epd_height );

extern void load_rgb565_raw_data( unsigned char *pAddr, const short * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height );
extern void load_gray_raw_data( unsigned char *pAddr, const char * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height );
extern void load_rgb565_raw_data_endian( unsigned char *pAddr, const short * rawData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height );
extern void load_fill_data( unsigned char *pAddr, char grayData , int img_x, int img_y, int img_width, int img_height, int epd_width, int epd_height );