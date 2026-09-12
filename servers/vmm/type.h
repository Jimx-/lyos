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

#ifndef _VMM_TYPE_H_
#define _VMM_TYPE_H_

#include <lyos/types.h>
#include <lyos/endpoint.h>

struct vmm_vm {
    int in_use;
    u32 id; /* server-side id, never reused */
    endpoint_t owner;
    u64 mem_size;
    void* ram;       /* shared VMM mapping of the guest RAM object */
    u32 mm_handle;   /* MM guest RAM handle (stale after teardown) */
    u32 vm_handle;   /* kernel HVCTL VM handle */
    u32 vcpu_handle; /* kernel HVCTL vCPU handle */
};

#endif /* _VMM_TYPE_H_ */
