#ifndef __ELISA_GPIO_H_
#define __ELISA_GPIO_H_

/* GPIO bank summary:
 *
 * Bank	GPIOs	Style	SlpCon	ExtInt Group
 * A		8		4Bit		Yes		1
 * B		7		4Bit		Yes		1
 * C		8		4Bit		Yes		2
 * D		5		4Bit		Yes		3
 * E		5		4Bit		Yes		None
 * F		16		2Bit		Yes		4 [1]
 * G		7		4Bit		Yes		5
 * H		10		4Bit[2]	Yes		6
 * I		16		2Bit		Yes		None
 * J		12		2Bit		Yes		None
 * K		16		4Bit[2]	No		None
 * L		15		4Bit[2] 	No		None
 * M		6		4Bit		No		IRQ_EINT
 * N		16		2Bit		No		IRQ_EINT
 * O		16		2Bit		Yes		7
 * P		15		2Bit		Yes		8
 * Q		9		2Bit		Yes		9
 */

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
	WM8731,
	GPNOTHING,
}GPIO_PORT;

typedef enum
{
	GPIO_INPUT,
	GPIO_OUTPUT,
}GPIO_DIRECTION;			//not use

typedef enum
{
	GPIO_PULL_NONE,
	GPIO_PULL_DOWN,
	GPIO_PULL_UP,
}GPIO_PULL;				//not use

typedef struct {
	unsigned int port_group;
	unsigned int port_num;
	unsigned int direction;		//not use
	unsigned int pull_updown;	//not use
	unsigned int value;
} egpio_status;

#define EGPIO_IOCTL_MAGIC		288
#define EGPIO_GET				_IOR(EGPIO_IOCTL_MAGIC, 0, egpio_status)
#define EGPIO_SET				_IOW(EGPIO_IOCTL_MAGIC, 1, egpio_status)
#define EGPIO_STATUS			_IO(EGPIO_IOCTL_MAGIC, 2)

#endif
