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
#include "lyos/config.h"
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <errno.h>
#include <lyos/portio.h>
#include <asm/page.h>
#include <asm/proto.h>

static char vportio_buf[VPORTIO_BUF_SIZE];
static pb_pair_t* pbp = (pb_pair_t*)vportio_buf;
static pw_pair_t* pwp = (pw_pair_t*)vportio_buf;
static pl_pair_t* plp = (pl_pair_t*)vportio_buf;

int sys_vportio(MESSAGE* m, struct proc* p_proc)
{
    int type = m->PIO_REQUEST & PIO_TYPE_MASK,
        dir = m->PIO_REQUEST & PIO_DIR_MASK;
    int vec_size = m->PIO_VECSIZE;
    int retval;

    if (vec_size <= 0) return EINVAL;

    int bytes;
    switch (type) {
    case PIO_BYTE:
        bytes = sizeof(pb_pair_t) * vec_size;
        break;
    case PIO_WORD:
        bytes = sizeof(pw_pair_t) * vec_size;
        break;
    case PIO_LONG:
        bytes = sizeof(pl_pair_t) * vec_size;
        break;
    default:
        return EINVAL;
    }

    if (bytes > VPORTIO_BUF_SIZE) return E2BIG;

    /* copy port-value pairs */
    if ((retval = data_vir_copy_check(p_proc, KERNEL, vportio_buf,
                                      p_proc->endpoint, m->PIO_BUF, bytes)) !=
        OK)
        return retval;

    int i;
    switch (type) {
    case PIO_BYTE:
        for (i = 0; i < vec_size; i++) {
            if (dir == PIO_IN)
                pbp[i].value = in_byte(pbp[i].port);
            else
                out_byte(pbp[i].port, pbp[i].value);
        }
        break;
    case PIO_WORD:
        for (i = 0; i < vec_size; i++) {
            if (dir == PIO_IN)
                pwp[i].value = in_word(pwp[i].port);
            else
                out_word(pwp[i].port, pwp[i].value);
        }
        break;
    case PIO_LONG:
        for (i = 0; i < vec_size; i++) {
            if (dir == PIO_IN)
                plp[i].value = in_long(plp[i].port);
            else
                out_long(plp[i].port, plp[i].value);
        }
        break;
    default:
        return EINVAL;
    }

    if (dir == PIO_IN) {
        if ((retval = data_vir_copy_check(p_proc, p_proc->endpoint, m->PIO_BUF,
                                          KERNEL, vportio_buf, bytes)) != OK)
            return retval;
    }

    return 0;
}
