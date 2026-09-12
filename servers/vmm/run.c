/*
    (c)Copyright 2026 Jimx

    This file is part of Lyos.

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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/vmm.h>
#include <lyos/sysutils.h>
#include <errno.h>
#include "proto.h"
#include "type.h"
#include "arch.h"

int do_vmm_vm_run(MESSAGE* m)
{
    struct mess_vmm* req = &m->u.m_vmm;
    struct vmm_vm* vm = vm_lookup(req->vm_id, m->source);
    struct vmm_run_status status;
    int retval;

    /* Reject old ABI layouts instead of returning a partially filled record. */
    if (!vm || !req->buf || req->buf_len != sizeof(status)) return EINVAL;
    if ((retval = vmm_arch_ops.run(vm, &status)) != 0) return retval;
    return data_copy(m->source, req->buf, SELF, &status, sizeof(status));
}
