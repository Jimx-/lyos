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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/bitmap.h>
#include <signal.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include <lyos/vm.h>
#include "global.h"

#define OBJ_ALIGN 4

#define SLABSIZE 200
#define MINSIZE 8
#define MAXSIZE ((SLABSIZE - 1 + MINSIZE / OBJ_ALIGN) * OBJ_ALIGN)

struct slabdata;

struct slabheader {
    struct list_head list;
    bitchunk_t used_mask[BITCHUNKS(ARCH_PG_SIZE / MINSIZE)];
    u16 freeguess;
    u16 used;
    phys_bytes phys;
    struct slabdata* data;
};

#define DATABYTES (ARCH_PG_SIZE - sizeof(struct slabheader))

struct slabdata {
    u8 data[DATABYTES];
    struct slabheader header;
};

static struct list_head slabs[SLABSIZE];

#define SLAB_INDEX(bytes) (roundup(bytes, OBJ_ALIGN) - (MINSIZE / OBJ_ALIGN))

void slabs_init()
{
    int i;
    for (i = 0; i < SLABSIZE; i++) {
        INIT_LIST_HEAD(&slabs[i]);
    }

    mem_info.slab = 0;
}

static struct slabdata* alloc_slabdata()
{
    phys_bytes phys;
    struct slabdata* sd =
        (struct slabdata*)alloc_vmem(&phys, ARCH_PG_SIZE, PGT_SLAB);
    if (!sd) return NULL;

    memset(&sd->header.used_mask, 0, sizeof(sd->header.used_mask));
    sd->header.used = 0;
    sd->header.freeguess = 0;
    INIT_LIST_HEAD(&(sd->header.list));
    sd->header.phys = phys;
    sd->header.data = sd;

    mem_info.slab += ARCH_PG_SIZE;

    return sd;
}

void* slaballoc(int bytes)
{
    if (bytes > MAXSIZE || bytes < MINSIZE) {
        printl("mm: slaballoc: invalid size(%d bytes)\n", bytes);
        return NULL;
    }

    struct list_head* slab = &slabs[SLAB_INDEX(bytes)];
    struct slabdata* sd;
    struct slabheader* header;

    bytes = roundup(bytes, OBJ_ALIGN);

    /* no slab ? */
    if (list_empty(slab)) {
        sd = alloc_slabdata();
        if (!sd) return NULL;

        list_add(&sd->header.list, slab);
    }

    int i;
    int max_objs = DATABYTES / bytes;
    /* try to find a slab with enough space */
    list_for_each_entry(header, slab, list)
    {
        if (header->used == max_objs) continue;

        for (i = header->freeguess; i < max_objs; i++) {
            if (!GET_BIT(header->used_mask, i)) {
                sd = header->data;
                SET_BIT(header->used_mask, i);
                header->freeguess = i + 1;
                header->used++;

                return (void*)&sd->data[i * bytes];
            }
        }

        for (i = 0; i < header->freeguess; i++) {
            if (!GET_BIT(header->used_mask, i)) {
                sd = header->data;
                SET_BIT(header->used_mask, i);
                header->freeguess = i + 1;
                header->used++;

                return (void*)&sd->data[i * bytes];
            }
        }
    }

    /* no space left, allocate new */
    sd = alloc_slabdata();
    if (!sd) return NULL;

    header = &sd->header;
    list_add(&header->list, slab);

    for (i = 0; i < max_objs; i++) {
        if (!GET_BIT(header->used_mask, i)) {
            SET_BIT(header->used_mask, i);
            header->freeguess = i + 1;
            header->used++;

            return (void*)&sd->data[i * bytes];
        }
    }

    return NULL;
}

void slabfree(void* mem, int bytes)
{
    if (bytes > MAXSIZE || bytes < MINSIZE) {
        return;
    }

    struct list_head* slab = &slabs[SLAB_INDEX(bytes)];
    struct slabdata* sd;
    struct slabheader* header;

    bytes = roundup(bytes, OBJ_ALIGN);

    /* no slab ? */
    if (list_empty(slab)) return;

    /* find the slab that contains this obj */
    list_for_each_entry(header, slab, list)
    {
        if (header->used == 0) continue;
        sd = header->data;

        if ((mem >= (void*)(&sd->data)) &&
            (mem < (void*)&sd->data + DATABYTES)) {
            int i = ((void*)mem - (void*)&sd->data) / bytes;
            UNSET_BIT(header->used_mask, i);
            header->used--;
            header->freeguess = i;
            return;
        }
    }
}
