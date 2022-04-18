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

#include <lyos/ipc.h>
#include "lyos/const.h"
#include <kernel/proto.h>
#include <errno.h>
#include <lyos/portio.h>
#include <asm/page.h>
#include <asm/proto.h>

int sys_portio(MESSAGE* m, struct proc* p_proc)
{
    int type = m->PIO_REQUEST & PIO_TYPE_MASK,
        dir = m->PIO_REQUEST & PIO_DIR_MASK;

    if (dir == PIO_IN) {
        switch (type) {
        case PIO_BYTE:
            m->PIO_VALUE = in_byte(m->PIO_PORT);
            break;
        case PIO_WORD:
            m->PIO_VALUE = in_word(m->PIO_PORT);
            break;
        case PIO_LONG:
            m->PIO_VALUE = in_long(m->PIO_PORT);
            break;
        default:
            return EINVAL;
        }
    } else {
        switch (type) {
        case PIO_BYTE:
            out_byte(m->PIO_PORT, m->PIO_VALUE);
            break;
        case PIO_WORD:
            out_word(m->PIO_PORT, m->PIO_VALUE);
            break;
        case PIO_LONG:
            out_long(m->PIO_PORT, m->PIO_VALUE);
            break;
        default:
            return EINVAL;
        }
    }

    return 0;
}
