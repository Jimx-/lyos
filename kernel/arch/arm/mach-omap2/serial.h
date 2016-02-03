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

#ifndef _MACH_OMAP2_SERIAL_H_
#define _MACH_OMAP2_SERIAL_H_

/* UART base */
#define OMAP3_BEAGLE_DEBUG_UART_BASE 0x49020000
#define OMAP3_AM335X_DEBUG_UART_BASE 0x44E09000

/* UART registers */
#define OMAP3_THR 0x000

#define OMAP3_LSR 0x014
#define OMAP3_LSR_TEMT    0x40
#define OMAP3_LSR_THRE    0x20

#define OMAP3_SSR 0x044

#endif
