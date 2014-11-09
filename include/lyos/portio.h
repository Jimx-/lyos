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

#ifndef _PORTIO_H_
#define _PORTIO_H_

#define PIO_TYPE_MASK   0x3
#define PIO_DIR_MASK    0x4

#define PIO_BYTE    0x1
#define PIO_WORD    0x2
#define PIO_LONG    0x3

#define PIO_IN    0
#define PIO_OUT   0x4

#define PIO_PORT    u.m3.m3i1
#define PIO_VECSIZE u.m3.m3i2
#define PIO_VALUE   u.m3.m3i2
#define PIO_REQUEST u.m3.m3i3
#define PIO_BUF     u.m3.m3p1

typedef struct { u16 port; u8 value; } pb_pair_t;
typedef struct { u16 port; u16 value; } pw_pair_t;
typedef struct { u16 port; u32 value; } pl_pair_t;

PUBLIC int portio_in(int port, u32 * value, int type);
PUBLIC int portio_out(int port, u32 value, int type);

#define portio_inb(p, v) portio_in(p, (u32 *)v, PIO_BYTE)
#define portio_inw(p, v) portio_in(p, (u32 *)v, PIO_WORD)
#define portio_inl(p, v) portio_in(p, (u32 *)v, PIO_LONG)

#define portio_outb(p, v) portio_out(p, (u32)v, PIO_BYTE)
#define portio_outw(p, v) portio_out(p, (u32)v, PIO_WORD)
#define portio_outl(p, v) portio_out(p, (u32)v, PIO_LONG)

#define pv_set(pair, p, v) do { \
                    (pair).port = (p);  \
                    (pair).value = (v); } while(0)

PUBLIC int portio_voutb(pb_pair_t * pairs, int nr_ports);

#endif
