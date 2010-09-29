

/*
elice hw key config.h
*/

/**************************************
1 byte: key type
***************************************/
#define KEY_TYPE_NULL			0x00
#define KEY_TYPE_DETECT		0x10
#define KEY_TYPE_POWER			0x30
#define KEY_TYPE_HOLD			0x50

#define KEY_TYPE_TOUCH			0x80
#define KEY_TYPE_G_SENSOR		0x90

/***************************************
2 byte
***************************************/

/*
 KEY_TYPE_DETECT	==> menu, prev, next
*/
#define KEY_DETECT_MENU_SHORT		0x11
#define KEY_DETECT_PREV_SHORT		0x12
#define KEY_DETECT_NEXT_SHORT		0x13

#define KEY_DETECT_MENU_LONG		0x16
#define KEY_DETECT_PREV_LONG		0x17
#define KEY_DETECT_NEXT_LONG		0x18

/*
 KEY_TYPE_POWER	==> power.
*/
#define KEY_POWER_SHORT		0x31
#define KEY_POWER_LONG			0x32

/*
special case.
*/
#define KEY_RELEASE		0x4f

/***************************************
2, 3 byte hold state.
***************************************/
/*
define KEY_TYPE_HOLD ==> hold on/off
*/
#define KEY_HOLD_ON			0x51
#define KEY_HOLD_OFF			0x50