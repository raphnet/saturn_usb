/* VBoy2USB: Virtual Boy to USB adapter
 * Copyright (C) 2009 Raphaël Assénat
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

#define REPORT_SIZE		6


// report matching the most recent bytes from the controller
static unsigned char last_built_report[REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_sent_report[REPORT_SIZE];

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

static char saturnChanged(void)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }
	
	return memcmp(last_built_report, 
					last_sent_report, REPORT_SIZE);
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

static char saturnReadAnalog(void)
{
	unsigned char dat[32];
	int i;
	char tr = 0;
	char r;
	char digital_mode = 0;
	int nibbles = 14;

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

	last_built_report[0] = 0x7f;
	last_built_report[1] = 0x7f;
	last_built_report[2] = 0x7f;
	last_built_report[3] = 0x7f;
	last_built_report[4] = 0;
	last_built_report[5] = 0;
	// dat[2]  : Up Dn Lf Rt
	// dat[3]  : B  C  A  St
	// dat[4]  : Z  Y  X  R
	// dat[5]  : ?  ?  ?  L
	
	if (!(dat[3] & 0x04)) // A
		last_built_report[4] |= 0x01;
	if (!(dat[3] & 0x01)) // B
		last_built_report[4] |= 0x02;
	if (!(dat[3] & 0x02)) // C
		last_built_report[4] |= 0x04;

	if (!(dat[4] & 0x04)) // X
		last_built_report[4] |= 0x08;
	if (!(dat[4] & 0x02)) // Y
		last_built_report[4] |= 0x10;
	if (!(dat[4] & 0x01)) // Z
		last_built_report[4] |= 0x20;

	if (!(dat[3] & 0x08)) // Start
		last_built_report[4] |= 0x40;

	if (!(dat[5] & 0x08)) // L
		last_built_report[4] |= 0x80;
	if (!(dat[4] & 0x08)) // R
		last_built_report[5] |= 0x01;
	
	if (digital_mode) {
		// switch is in the "+" position
		if (!(dat[2] & 0x08)) // right
			last_built_report[0] = 0xff;
		if (!(dat[2] & 0x04)) // left
			last_built_report[0] = 0x00;
		if (!(dat[2] & 0x02)) // down
			last_built_report[1] = 0xff;
		if (!(dat[2] & 0x01)) // Up
			last_built_report[1] = 0x00;
	}
	else {
		if (!(dat[2] & 0x08)) // Right
			last_built_report[5] |= 0x02;
		if (!(dat[2] & 0x04)) // Left
			last_built_report[5] |= 0x04;
		if (!(dat[2] & 0x02)) // Down
			last_built_report[5] |= 0x08;
		if (!(dat[2] & 0x01)) // Up
			last_built_report[5] |= 0x10;

		// switch is in the "o" position
		last_built_report[0] = (dat[7] & 0xf) | (dat[6] << 4);
		last_built_report[1] = (dat[9] & 0xf) | (dat[8] << 4);
		last_built_report[2] = (dat[11] & 0xf) | (dat[10] << 4);
		last_built_report[3] = (dat[13] & 0xf) | (dat[12] << 4);
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


	last_built_report[0] = 0x7f;
	last_built_report[1] = 0x7f;
	last_built_report[2] = 0x7f;
	last_built_report[3] = 0x7f;
	last_built_report[4] = 0;
	last_built_report[5] = 0;

	if (!(c & 0x08)) // right
		last_built_report[0] = 0xff;
	if (!(c & 0x04)) // left
		last_built_report[0] = 0x00;
	if (!(c & 0x02)) // down
		last_built_report[1] = 0xff;
	if (!(c & 0x01)) // Up
		last_built_report[1] = 0x00;

	if (!(b & 0x04)) // A
		last_built_report[4] |= 0x01;
	if (!(b & 0x01)) // B
		last_built_report[4] |= 0x02;
	if (!(b & 0x02)) // C
		last_built_report[4] |= 0x04;

	if (!(a & 0x04)) // X
		last_built_report[4] |= 0x08;
	if (!(a & 0x02)) // Y
		last_built_report[4] |= 0x10;
	if (!(a & 0x01)) // Z
		last_built_report[4] |= 0x20;

	if (!(b & 0x08)) // Start
		last_built_report[4] |= 0x40;

	if (!(d & 0x08)) // L
		last_built_report[4] |= 0x80;
	if (!(a & 0x08)) // R
		last_built_report[5] |= 0x01;
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
		saturnReadAnalog();	
		return;
	}
	
	// Bit 4-0: 1L100 where 'L' is the 'L' button status
	if ((tmp & 0x17) == 0x14) {
		saturnReadPad();
		return;
	}

/*
	// mouse	
	if (tmp == 0x10) {
	}
*/

	// default idle
	last_built_report[0] = 0x7f;
	last_built_report[1] = 0x7f;
	last_built_report[2] = 0x7f;
	last_built_report[3] = 0x7f;
	last_built_report[4] = 0;
	last_built_report[5] = 0;

}

static void saturnBuildReport(unsigned char *reportBuffer)
{
	if (reportBuffer != NULL)
	{
		memcpy(reportBuffer, last_built_report, REPORT_SIZE);
	}
	memcpy(last_sent_report, 
			last_built_report, 
			REPORT_SIZE);	
}

#include "report_desc_4axes_16btns.c"

Gamepad saturnGamepad = {
	report_size: 		REPORT_SIZE,
	reportDescriptorSize:	sizeof(usbHidReportDescriptor_4axes_16btns),
	init: 			saturnInit,
	update: 		saturnUpdate,
	changed:		saturnChanged,
	buildReport:		saturnBuildReport
};

Gamepad *saturnGetGamepad(void)
{
	saturnGamepad.reportDescriptor = (void*)usbHidReportDescriptor_4axes_16btns;

	return &saturnGamepad;
}

