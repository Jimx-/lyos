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

#ifndef _VMM_PROTO_H_
#define _VMM_PROTO_H_

#include <lyos/types.h>
#include <lyos/ipc.h>

/* main.c */
extern int vmm_available;
extern struct hv_caps vmm_hw_caps;

/* vm.c */
struct vmm_vm* vm_lookup(u32 id, endpoint_t owner);
void vm_reap_dead_clients(void);
int do_vmm_query(MESSAGE* m);
int do_vmm_vm_create(MESSAGE* m);
int do_vmm_vm_destroy(MESSAGE* m);
int do_vmm_vm_write(MESSAGE* m);
int do_vmm_vm_read(MESSAGE* m);
int do_vmm_vm_run(MESSAGE* m);

#endif /* _VMM_PROTO_H_ */
