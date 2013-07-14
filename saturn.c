/* Saturn to USB : Sega saturn controllers to USB adapter
 * Copyright (C) 2011-2013 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbdrv.h"
#include "gamepad.h"
#include "saturn.h"

#define MAX_REPORT_SIZE			6
#define NUM_REPORTS				2

#define JOYSTICK_REPORT_IDX		0
#define JOYSTICK_REPORT_SIZE	6
/*
 * x
 * y
 * rx
 * rz
 * buttons 0-7
 * buttons 8-15
 **/

#define MOUSE_REPORT_IDX		1
#define MOUSE_REPORT_SIZE		3	
/*
 * buttons
 * x
 * y
 **/

// report matching the most recent bytes from the controller
static unsigned char last_built_report[NUM_REPORTS][MAX_REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_sent_report[NUM_REPORTS][MAX_REPORT_SIZE];

static char report_sizes[NUM_REPORTS] = { JOYSTICK_REPORT_SIZE, MOUSE_REPORT_SIZE };
static char g_mouse_detected = 0;
static char g_mouse_mode = 0;
static Gamepad saturnGamepad;

static void saturnUpdate(void);

/*
 * [0] X
 * [1] Y
 * [2] Rx
 * [3] Rz
 * [4] Btn 0-7
 * [5] Btn 8-15 
 * [6] Btn 16-23
 */
static const unsigned char saturnPadReport[] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game pad)
    0xa1, 0x01,                    // COLLECTION (Application)
	0x09, 0x01,                    //   USAGE (Pointer)    
	0xa1, 0x00,                    //   COLLECTION (Physical)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
	0x09, 0x36,					   //     USAGE (Rx)
	0x09, 0x37,						//	  USAGE (Rz)	
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x04,                    //   REPORT_COUNT (4)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
	0x05, 0x09,                    // USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x10,                    //   USAGE_MAXIMUM (Button 16)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    // REPORT_SIZE (1)
    0x95, 0x10,                    // REPORT_COUNT (16)
    0x81, 0x02,                    // INPUT (Data,Var,Abs)
    0xc0,                          // END_COLLECTION
    0xc0,                          // END_COLLECTION
};

/*
 * [6] Mouse buttons
 * [7] Mouse X
 * [8] Mouse Y
 */
static const unsigned char saturnMouseReport[] PROGMEM = {
	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x04,                    //     REPORT_SIZE (4)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0,                          // END_COLLECTION
};

const unsigned char saturnMouseDevDesc[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
	0x9B, 0x28,	// Vendor ID
    0x06, 0x00, // Product ID
	0x00, 0x02, // Version: Minor, Major
	1,	// Manufacturer String
	2, // Product string
	3,  // Serial number string
    1,          /* number of configurations */
};


#define TR_HIGH()	PORTC |= (1<<4)
#define TR_LOW()	PORTC &= ~(1<<4)
#define TH_HIGH()	PORTC |= (1<<5)
#define TH_LOW()	PORTC &= ~(1<<5)

static inline unsigned char getDat()
{
	unsigned char t = 0;
	unsigned char p = PINC;

	t = (PINB & 0x20) >> 1;
	t |= (p & 0x08) >> 3;
	t |= (p & 0x04) >> 1;
	t |= (p & 0x02) << 1;
	t |= (p & 0x01) << 3;

	return t;
}

static void saturnInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();
	
	// PORTC | Name | Function | Dir
	//  5    |  S0  | TH       | Out
	//  4    |  S1  | TR       | Out
	//  3    |  D0  | Up       | In
	//  2    |  D1  | Down     | In
	//  1    |  D2  | Left     | In
	//  0    |  D3  | Right    | In
	//
	// PORTB |
	//  5    |  D4? | TL       | In

	DDRC = 0x30;
	PORTC = 0xff; // default high, pull-up enabled on input
	DDRB = 0;
	PORTB = 0xff;

	SREG = sreg;

	saturnUpdate();

	if (g_mouse_detected) {
		saturnGamepad.reportDescriptor = (void*)saturnMouseReport;
		saturnGamepad.reportDescriptorSize = sizeof(saturnMouseReport);
		saturnGamepad.deviceDescriptor = (void*)saturnMouseDevDesc;
		saturnGamepad.deviceDescriptorSize = sizeof(saturnMouseDevDesc);
		g_mouse_mode = 1;
	} else {
		saturnGamepad.reportDescriptor = (void*)saturnPadReport;
		saturnGamepad.reportDescriptorSize = sizeof(saturnPadReport);
	}
}



static char inline waitTL(char state)
{
	char t_out = 100;
	if (state) {
		while(!(getDat() & 0x10)) {
			_delay_us(1);
			t_out--;
			if (!t_out)
				return -1;
		}
	} else {
		while((getDat() & 0x10)) {
			_delay_us(1);
			t_out--;
			if (!t_out)
				return -1;
		}
	}
	return 0;
}

static void idleJoystick(void)
{
	unsigned char *joy_report = last_built_report[JOYSTICK_REPORT_IDX];
	joy_report[0] = 0x7F;
	joy_report[1] = 0x7F;
	joy_report[2] = 0x7F;
	joy_report[3] = 0x7F;
	joy_report[4] = 0;
	joy_report[5] = 0;
}

static void idleMouse(void)
{
	unsigned char *mouse_report = last_built_report[MOUSE_REPORT_IDX];
	memset(mouse_report, 0, MAX_REPORT_SIZE);
}

#define MAPPING_UNDEFINED	0

#define MAPPING_SLS			1
#define MAPPING_SLS_ALT		2	// Saturn start -> PS3 select
#define MAPPING_VIP			3
#define MAPPING_TEST		4
#define MAPPING_IDENTITY	5

static char current_mapping = MAPPING_UNDEFINED;

static void permuteButtons(void)
{
	unsigned int buttons_in, buttons_out;
	unsigned char *joy_report = last_built_report[JOYSTICK_REPORT_IDX];
	int i;

	/* Saturn		:   A  B  C  X  Y  Z  S  L  R */		 
	char sls[9]		= { 1, 2, 5, 0, 3, 4, 9, 6, 7 };
	char sls_alt[9]	= { 1, 2, 5, 0, 3, 4, 8, 6, 7 };
	char vip[9] 	= { 0, 1, 2, 3, 4, 5, 8, 6, 7 };
	char *permuter;

	buttons_in = joy_report[4];
	buttons_in |= joy_report[5] << 8;
	

	/* Only run once. Hold A or B at power-up to select mappings. */
	if (current_mapping == MAPPING_UNDEFINED) {
		
		current_mapping = MAPPING_SLS; // default. 

		if (buttons_in & 0x01) // A
			current_mapping = MAPPING_SLS_ALT;
		if (buttons_in & 0x02) // B
			current_mapping = MAPPING_VIP;
		if (buttons_in & 0x04) // C
			current_mapping = MAPPING_IDENTITY;
	}

	switch(current_mapping)
	{
		default:
		case MAPPING_IDENTITY:
			return;

		case MAPPING_SLS:
			permuter = sls;
			break;

		case MAPPING_SLS_ALT:
			permuter = sls_alt;
			break;

		case MAPPING_VIP:
			permuter = vip;
			break;
	}	

	// Let the analog pad D-Pad buttons 
	// pass through by masking only those
	// handled here.
	buttons_out = buttons_in;
	buttons_out &= ~0x1FF;

	for (i=0; i<9; i++) {
		if (buttons_in & (1<<i)) {
			buttons_out |= (1<<permuter[i]);
		}
	}

	joy_report[4] = buttons_out;
	joy_report[5] = buttons_out >> 8;
}

static char saturnReadMouse(void)
{
	unsigned char dat[32];
	int i;
	char tr = 0;
	char r;
	char digital_mode = 0;
	int nibbles = 8;
	unsigned char *mouse_report = last_built_report[MOUSE_REPORT_IDX];
	unsigned char x,y;

	_delay_us(4);
	TH_LOW();
	_delay_us(4);

	for (i=0; i<nibbles; i++) {
		if (tr) {
			TR_HIGH();
			r = waitTL(1);
			if (r) 
				return -1;
		}
		else {
			TR_LOW();
			r = waitTL(0);
			if (r)
				return -1;
		}

		_delay_us(2);

		dat[i] = getDat();

		tr ^= 1;

		if (i>=1) {
			if (dat[1] == 0x12) {
				digital_mode = 1;
				nibbles = 8;
			}
		}
	}

	TR_HIGH();
	_delay_us(4);
	TH_HIGH();
	_delay_us(4);

	idleMouse();

	if (dat[3] & 0x01)
		mouse_report[0] |= 1;
	if (dat[3] & 0x02)
		mouse_report[0] |= 2;
	if (dat[3] & 0x04)
		mouse_report[0] |= 4;
	if (dat[3] & 0x08)
		mouse_report[0] |= 8;

	x = (dat[5]&0xf) | (dat[4]<<4);
	y = (dat[7]&0xf) | (dat[6]<<4);

	mouse_report[1] = x;	
	mouse_report[2] = 256 - y;	

	return 0;
}


static char saturnReadAnalog(void)
{
	unsigned char dat[32];
	int i;
	char tr = 0;
	char r;
	char digital_mode = 0;
	int nibbles = 14;
	unsigned char *joy_report = last_built_report[JOYSTICK_REPORT_IDX];

	_delay_us(4);
	TH_LOW();
	_delay_us(4);

	for (i=0; i<nibbles; i++) {
		if (tr) {
			TR_HIGH();
			r = waitTL(1);
			if (r) 
				return -1;
		}
		else {
			TR_LOW();
			r = waitTL(0);
			if (r)
				return -1;
		}

		_delay_us(2);

		dat[i] = getDat();

		tr ^= 1;

		if (i>=1) {
			if (dat[1] == 0x12) {
				digital_mode = 1;
				nibbles = 8;
			}
		}
	}

	idleJoystick();
	// dat[2]  : Up Dn Lf Rt
	// dat[3]  : B  C  A  St
	// dat[4]  : Z  Y  X  R
	// dat[5]  : ?  ?  ?  L
	
	if (!(dat[3] & 0x04)) // A
		joy_report[4] |= 0x01;
	if (!(dat[3] & 0x01)) // B
		joy_report[4] |= 0x02;
	if (!(dat[3] & 0x02)) // C
		joy_report[4] |= 0x04;

	if (!(dat[4] & 0x04)) // X
		joy_report[4] |= 0x08;
	if (!(dat[4] & 0x02)) // Y
		joy_report[4] |= 0x10;
	if (!(dat[4] & 0x01)) // Z
		joy_report[4] |= 0x20;

	if (!(dat[3] & 0x08)) // Start
		joy_report[4] |= 0x40;

	if (!(dat[5] & 0x08)) // L
		joy_report[4] |= 0x80;
	if (!(dat[4] & 0x08)) // R
		joy_report[5] |= 0x01;
	
	if (digital_mode) {
		// switch is in the "+" position
		if (!(dat[2] & 0x08)) // right
			joy_report[0] = 0xff;
		if (!(dat[2] & 0x04)) // left
			joy_report[0] = 0x00;
		if (!(dat[2] & 0x02)) // down
			joy_report[1] = 0xff;
		if (!(dat[2] & 0x01)) // Up
			joy_report[1] = 0x00;
	}
	else {
		if (!(dat[2] & 0x08)) // Right
			joy_report[5] |= 0x04;
		if (!(dat[2] & 0x04)) // Left
			joy_report[5] |= 0x08;
		if (!(dat[2] & 0x02)) // Down
			joy_report[5] |= 0x10;
		if (!(dat[2] & 0x01)) // Up
			joy_report[5] |= 0x20;

		// switch is in the "o" position
		joy_report[0] = (dat[7] & 0xf) | (dat[6] << 4);
		joy_report[1] = (dat[9] & 0xf) | (dat[8] << 4);
		joy_report[2] = (dat[11] & 0xf) | (dat[10] << 4);
		joy_report[3] = (dat[13] & 0xf) | (dat[12] << 4);
	} 
	

	TR_HIGH();
	_delay_us(4);
	TH_HIGH();
	_delay_us(4);

	return 0;
}

static void saturnReadPad(void)
{
	unsigned char a,b,c,d;
	unsigned char *joy_report = last_built_report[JOYSTICK_REPORT_IDX];
	

	// TH and TR already high from detecting, read this
	// nibble first! Otherwise the HORIPAD SS (HSS-11) does
	// not work! The Performance Super Pad 8 does though.
	//
	// d0 d1 d2 d3
	// 0  0  1  L	
	TH_HIGH();
	TR_HIGH();
	_delay_us(4);
	d = getDat();


	// d0 d1 d2 d3
	// Z  Y  X  R	
	TH_LOW();
	TR_LOW();
	_delay_us(4);
	a = getDat();

	// d0 d1 d2 d3
	// B  C  A  St	
	TH_HIGH();
	TR_LOW();
	_delay_us(4);
	b = getDat();

	// d0 d1 d2 d3
	// UP DN LT RT	
	TH_LOW();
	TR_HIGH();
	_delay_us(4);
	c = getDat();

	idleJoystick();

	if (!(c & 0x08)) // right
		joy_report[0] = 0xff;
	if (!(c & 0x04)) // left
		joy_report[0] = 0x00;
	if (!(c & 0x02)) // down
		joy_report[1] = 0xff;
	if (!(c & 0x01)) // Up
		joy_report[1] = 0x00;

	if (!(b & 0x04)) // A
		joy_report[4] |= 0x01;
	if (!(b & 0x01)) // B
		joy_report[4] |= 0x02;
	if (!(b & 0x02)) // C
		joy_report[4] |= 0x04;

	if (!(a & 0x04)) // X
		joy_report[4] |= 0x08;
	if (!(a & 0x02)) // Y
		joy_report[4] |= 0x10;
	if (!(a & 0x01)) // Z
		joy_report[4] |= 0x20;

	if (!(b & 0x08)) // Start
		joy_report[4] |= 0x40;

	if (!(d & 0x08)) // L
		joy_report[4] |= 0x80;
	if (!(a & 0x08)) // R
		joy_report[5] |= 0x01;
}

static void saturnUpdate(void)
{
	char tmp;

	TH_HIGH();
	TR_HIGH();
	_delay_us(4);

	tmp = getDat();

	if (tmp == 0x11) {	
		idleMouse();
		if (0 == saturnReadAnalog()) {
			permuteButtons();
		}
		return;
	}
	
	// Bit 4-0: 1L100 where 'L' is the 'L' button status
	if ((tmp & 0x17) == 0x14) {
		idleMouse();
		saturnReadPad();
		permuteButtons();
		return;
	}

	// mouse	
	if (tmp == 0x10) {
		idleJoystick();
		saturnReadMouse();
		g_mouse_detected = 1;
		return;
	}

	// default idle
	idleJoystick();
	idleMouse();
}

static char saturnBuildReport(unsigned char *reportBuffer, unsigned char report_id)
{
	if (g_mouse_mode) {
		report_id = MOUSE_REPORT_IDX;
	} else {
		report_id = JOYSTICK_REPORT_IDX;
	}

	if (reportBuffer != NULL)
	{
		memcpy(reportBuffer, last_built_report[report_id], report_sizes[report_id]);
	}
	memcpy(last_sent_report[report_id], last_built_report[report_id], 
			report_sizes[report_id]);	

	return report_sizes[report_id];
}

static char saturnChanged(unsigned char report_id)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }

	if (g_mouse_mode) {
		report_id = MOUSE_REPORT_IDX;
	} else {
		report_id = JOYSTICK_REPORT_IDX;
	}

	return memcmp(last_built_report[report_id], last_sent_report[report_id], 
					report_sizes[report_id]);
}



static Gamepad saturnGamepad = {
	num_reports: 		1,
	init: 				saturnInit,
	update: 			saturnUpdate,
	changed:			saturnChanged,
	buildReport:		saturnBuildReport
};

Gamepad *saturnGetGamepad(void)
{
	return &saturnGamepad;
}

