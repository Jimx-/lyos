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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include <lyos/ipc.h>
#include <lyos/portio.h>

int syscall_entry(int syscall_nr, MESSAGE * m);

PUBLIC int portio_in(int port, u32 * value, int type)
{
    MESSAGE m;
    m.PIO_REQUEST = PIO_IN | type;
    m.PIO_PORT = port;

    int retval = syscall_entry(NR_PORTIO, &m);

    if (type == PIO_BYTE) 
        *(u8*)value = (u8)m.PIO_VALUE;
    else
        *value = m.PIO_VALUE;

    return retval;
}

PUBLIC int portio_out(int port, u32 value, int type)
{
    MESSAGE m;
    m.PIO_REQUEST = PIO_OUT | type;
    m.PIO_PORT = port;
    m.PIO_VALUE = value;

    return syscall_entry(NR_PORTIO, &m);
}

PUBLIC int portio_voutb(pb_pair_t * pairs, int nr_ports)
{
    MESSAGE m;
    m.PIO_REQUEST = PIO_OUT | PIO_BYTE;
    m.PIO_VECSIZE = nr_ports;
    m.PIO_BUF = pairs;

    return syscall_entry(NR_VPORTIO, &m);
}

PUBLIC int portio_sin(int port, void * buf, int len)
{
    MESSAGE m;
    m.PIO_REQUEST = PIO_IN;
    m.PIO_BUF = buf;
    m.PIO_BUFLEN = len;
    m.PIO_PORT = port;

    return syscall_entry(NR_SPORTIO, &m);
}

PUBLIC int portio_sout(int port, void * buf, int len)
{
    MESSAGE m;
    m.PIO_REQUEST = PIO_OUT;
    m.PIO_BUF = buf;
    m.PIO_BUFLEN = len;
    m.PIO_PORT = port;

    return syscall_entry(NR_SPORTIO, &m);
}
