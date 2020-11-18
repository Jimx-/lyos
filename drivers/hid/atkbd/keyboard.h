/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

/* AT keyboard */
/* 8042 ports */
/* I/O port for keyboard data
 Read : Read Output Buffer
 Write: Write Input Buffer(8042 Data&8048 Command) */
#define KB_DATA 0x60
/* I/O port for keyboard command
Read : Read Status Register
Write: Write Input Buffer(8042 Command) */
#define KB_CMD    0x64
#define KB_STATUS 0x64

#define KBC_RD_RAM_CCB 0x20
#define KBC_WR_RAM_CCB 0x60
#define KBC_AUX_DI     0xA7
#define KBC_AUX_EN     0xA8
#define KBC_KBD_DI     0xAD
#define KBC_KBD_EN     0xAE
#define LED_CODE       0xED

#define KB_ACK      0xFA
#define KB_OUT_FULL 0x01
#define KB_IN_FULL  0x02
#define KB_AUX_BYTE 0x20

#define KBC_IN_DELAY 7

#endif /* _KEYBOARD_H_ */
