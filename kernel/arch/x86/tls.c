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
#include <asm/protect.h>
#include <lyos/const.h>
#include <kernel/proc.h>
#include <string.h>
#include <errno.h>
#include <kernel/proto.h>
#include <asm/proto.h>
#include <asm/ldt.h>

static int get_free_idx(struct proc* p)
{
    int idx;

    for (idx = 0; idx < GDT_TLS_ENTRIES; idx++) {
        if (descriptor_empty(&p->seg.tls_array[idx]))
            return idx + INDEX_TLS_MIN;
    }

    return -ESRCH;
}

static void set_tls_desc(struct proc* p, int idx, const struct user_desc* info)
{
    struct descriptor* desc = &p->seg.tls_array[idx - INDEX_TLS_MIN];

    if (!info->base_addr)
        memset(desc, 0, sizeof(*desc));
    else
        init_desc(desc, info->base_addr, 0xfffff,
                  DA_32 | DA_LIMIT_4K | DA_DRW | PRIVILEGE_USER << 5);
}

int do_set_thread_area(struct proc* p, int idx, void* u_info, int can_allocate,
                       struct proc* caller)
{
    struct user_desc desc;
    int retval;

    if ((retval = data_vir_copy_check(caller, KERNEL, &desc, caller->endpoint,
                                      u_info, sizeof(desc))) != 0)
        return retval;

    if (idx == -1) idx = desc.entry_number;

    if (idx == -1 && can_allocate) {
        idx = get_free_idx(p);
        if (idx < 0) return -idx;

        desc.entry_number = idx;
        if ((retval = data_vir_copy_check(caller, caller->endpoint, u_info,
                                          KERNEL, &desc, sizeof(desc))) != 0)
            return retval;
    }

    if (idx < INDEX_TLS_MIN || idx > INDEX_TLS_MAX) return EINVAL;

    set_tls_desc(p, idx, &desc);

    return 0;
}

int sys_set_thread_area(MESSAGE* m, struct proc* p_proc)
{
    return do_set_thread_area(p_proc, -1, m->ADDR, TRUE /* can_allocate */,
                              p_proc);
}
