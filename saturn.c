/* Saturn to USB : Sega saturn controllers to USB adapter
 * Copyright (C) 2011 Raphaël Assénat
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
#include "gamepad.h"
#include "saturn.h"

#define MAX_REPORT_SIZE			9
#define NUM_REPORTS				2

#define JOYSTICK_REPORT_ID		1
#define JOYSTICK_REPORT_IDX		(JOYSTICK_REPORT_ID-1)
#define JOYSTICK_REPORT_SIZE	7
/*
 * report id [1]
 * x
 * y
 * rx
 * rz
 * buttons 0-7
 * buttons 8-15
 **/


#define MOUSE_REPORT_ID			2
#define MOUSE_REPORT_IDX		(MOUSE_REPORT_ID-1)
#define MOUSE_REPORT_SIZE		4	
/*
 * report id [2]
 * butons
 * x
 * y
 **/

// report matching the most recent bytes from the controller
static unsigned char last_built_report[NUM_REPORTS][MAX_REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_sent_report[NUM_REPORTS][MAX_REPORT_SIZE];

static char report_sizes[NUM_REPORTS] = { JOYSTICK_REPORT_SIZE, MOUSE_REPORT_SIZE };

Gamepad saturnGamepad;

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
	joy_report[0] = JOYSTICK_REPORT_ID;
	joy_report[1] = 0x7f;
	joy_report[2] = 0x7f;
	joy_report[3] = 0x7f;
	joy_report[4] = 0x7f;
	joy_report[5] = 0;
	joy_report[6] = 0;
}

static void idleMouse(void)
{
	unsigned char *mouse_report = last_built_report[MOUSE_REPORT_IDX];
	memset(mouse_report, 0, MAX_REPORT_SIZE);
	mouse_report[0] = MOUSE_REPORT_ID;
}

#define MAPPING_UNDEFINED	0

#define MAPPING_SLS			1
#define MAPPING_SLS_ALT		2	// Saturn start -> PS3 select
#define MAPPING_VIP			3
#define MAPPING_IDENTITY	4

static char current_mapping = MAPPING_UNDEFINED;

static void permuteButtons(void)
{
	unsigned short buttons_in, buttons_out;
	unsigned char *joy_report = last_built_report[JOYSTICK_REPORT_IDX];
	int i;

	/* Saturn		:   A  B  C  X  Y  Z  S  L  R */		 
	char sls[9]		= { 1, 2, 5, 0, 3, 4, 9, 6, 7 };
	char sls_alt[9]	= { 1, 2, 5, 0, 3, 4, 8, 6, 7 };
	char vip[9] 	= { 0, 1, 2, 3, 4, 5, 8, 6, 7 };
	char *permuter;

	buttons_in = joy_report[5];
	buttons_in |= joy_report[6] << 8;
	

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

	buttons_out = buttons_in;
	buttons_out &= ~0x1FF; // clear buttons

	for (i=0; i<9; i++) {
		if (buttons_in & (1<<i)) {
			buttons_out |= (1<<permuter[i]);
		}
	}

	joy_report[5] = buttons_out;
	joy_report[6] = buttons_out >> 8;
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
	//unsigned char *joy = last_built_report[JOYSTICK_REPORT_IDX];
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
		mouse_report[1] |= 1;
	if (dat[3] & 0x02)
		mouse_report[1] |= 2;
	if (dat[3] & 0x04)
		mouse_report[1] |= 4;
	if (dat[3] & 0x08)
		mouse_report[1] |= 8;

	x = (dat[5]&0xf) | (dat[4]<<4);
	y = (dat[7]&0xf) | (dat[6]<<4);

	mouse_report[2] = x;	
	mouse_report[3] = 256 - y;	

//	joy[5] = x;
//	joy[6] = y;

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
		joy_report[5] |= 0x01;
	if (!(dat[3] & 0x01)) // B
		joy_report[5] |= 0x02;
	if (!(dat[3] & 0x02)) // C
		joy_report[5] |= 0x04;

	if (!(dat[4] & 0x04)) // X
		joy_report[5] |= 0x08;
	if (!(dat[4] & 0x02)) // Y
		joy_report[5] |= 0x10;
	if (!(dat[4] & 0x01)) // Z
		joy_report[5] |= 0x20;

	if (!(dat[3] & 0x08)) // Start
		joy_report[5] |= 0x40;

	if (!(dat[5] & 0x08)) // L
		joy_report[5] |= 0x80;
	if (!(dat[4] & 0x08)) // R
		joy_report[6] |= 0x01;
	
	if (digital_mode) {
		// switch is in the "+" position
		if (!(dat[2] & 0x08)) // right
			joy_report[1] = 0xff;
		if (!(dat[2] & 0x04)) // left
			joy_report[1] = 0x00;
		if (!(dat[2] & 0x02)) // down
			joy_report[2] = 0xff;
		if (!(dat[2] & 0x01)) // Up
			joy_report[2] = 0x00;
	}
	else {
		if (!(dat[2] & 0x08)) // Right
			joy_report[6] |= 0x04;
		if (!(dat[2] & 0x04)) // Left
			joy_report[6] |= 0x08;
		if (!(dat[2] & 0x02)) // Down
			joy_report[6] |= 0x10;
		if (!(dat[2] & 0x01)) // Up
			joy_report[6] |= 0x20;

		// switch is in the "o" position
		joy_report[1] = (dat[7] & 0xf) | (dat[6] << 4);
		joy_report[2] = (dat[9] & 0xf) | (dat[8] << 4);
		joy_report[3] = (dat[11] & 0xf) | (dat[10] << 4);
		joy_report[4] = (dat[13] & 0xf) | (dat[12] << 4);
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
		joy_report[1] = 0xff;
	if (!(c & 0x04)) // left
		joy_report[1] = 0x00;
	if (!(c & 0x02)) // down
		joy_report[2] = 0xff;
	if (!(c & 0x01)) // Up
		joy_report[2] = 0x00;

	if (!(b & 0x04)) // A
		joy_report[5] |= 0x01;
	if (!(b & 0x01)) // B
		joy_report[5] |= 0x02;
	if (!(b & 0x02)) // C
		joy_report[5] |= 0x04;

	if (!(a & 0x04)) // X
		joy_report[5] |= 0x08;
	if (!(a & 0x02)) // Y
		joy_report[5] |= 0x10;
	if (!(a & 0x01)) // Z
		joy_report[5] |= 0x20;

	if (!(b & 0x08)) // Start
		joy_report[5] |= 0x40;

	if (!(d & 0x08)) // L
		joy_report[5] |= 0x80;
	if (!(a & 0x08)) // R
		joy_report[6] |= 0x01;
}

static void saturnUpdate(void)
{
	char tmp;

	TH_HIGH();
	TR_HIGH();
	_delay_us(4);

	tmp = getDat();

#if 0
 	// detection debugging
	last_built_report[0] = 0x7f;
	last_built_report[1] = 0x7f;
	last_built_report[2] = 0x7f;
	last_built_report[3] = 0x7f;
	last_built_report[4] = tmp;
	last_built_report[5] = 0;
#endif

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
		return;
	}

	// default idle
	idleJoystick();
	idleMouse();
}

static char saturnBuildReport(unsigned char *reportBuffer, unsigned char report_id)
{
	if (report_id < 1 || report_id > 2)
		return 0;

	// Translate report IDs (starting at 1) to array index
	report_id--;

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

	if (report_id < 1 || report_id > NUM_REPORTS)
		return 0;

	// Translate report IDs (starting at 1) to array index
	report_id--;

	return memcmp(last_built_report[report_id], last_sent_report[report_id], 
					report_sizes[report_id]);
}

#include "report_4a_16b_mouse4.c"

Gamepad saturnGamepad = {
	num_reports: 		2,
	reportDescriptorSize:	sizeof(usbHidReportDescriptor_4axes_16btns),
	init: 				saturnInit,
	update: 			saturnUpdate,
	changed:			saturnChanged,
	buildReport:		saturnBuildReport
};

Gamepad *saturnGetGamepad(void)
{
	saturnGamepad.reportDescriptor = (void*)usbHidReportDescriptor_4axes_16btns;

	return &saturnGamepad;
}

