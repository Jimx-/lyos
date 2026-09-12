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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <string.h>
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/page.h>
#include <lyos/bitmap.h>
#include "vmx.h"
#include "vmx_priv.h"

#define EPT_POOL_WORDS BITCHUNKS(VMX_EPT_POOL_PAGES)
#define EPT_ADDR_MASK  0x000ffffffffff000ULL
#define EPT_ENTRIES    512

static u8 vmx_ept_pool[VMX_EPT_POOL_PAGES][ARCH_PG_SIZE]
    __attribute__((aligned(ARCH_PG_SIZE)));
static bitchunk_t ept_pool_map[EPT_POOL_WORDS];
static u32 ept_pages_used;

/* EPT entry permission bits: 0 = R, 1 = W, 2 = X */
#define EPT_PERM(mask) ((mask) & 7)

static s32 ept_alloc_page(void)
{
    int i;
    for (i = 0; i < VMX_EPT_POOL_PAGES; i++) {
        if (!GET_BIT(ept_pool_map, i)) {
            SET_BIT(ept_pool_map, i);
            ept_pages_used++;
            memset(vmx_ept_pool[i], 0, ARCH_PG_SIZE);
            return i;
        }
    }
    return -1;
}

static void ept_free_page(int i)
{
    if (i < 0 || i >= VMX_EPT_POOL_PAGES) return;
    UNSET_BIT(ept_pool_map, i);
    if (ept_pages_used) ept_pages_used--;
}

u32 vmx_ept_pages_in_use(void) { return ept_pages_used; }

int vmx_ept_supported(void)
{
    u64 cap = vmx_state.ept_vpid_cap;

    /* WB memory type, 4-level walks, and a local INVEPT operation */
    if (!(cap & EPT_CAP_WB)) return 0;
    if (!(cap & EPT_CAP_WALK_4)) return 0;
    if (!(cap & EPT_CAP_INVEPT)) return 0;
    if (!(cap & (EPT_CAP_INVEPT_SINGLE | EPT_CAP_INVEPT_ALL))) return 0;
    return 1;
}

static inline int invept(u32 type, u64 eptp, u64 reserved)
{
    struct {
        u64 eptp;
        u64 reserved;
    } desc;
    u8 fail;

    desc.eptp = eptp;
    desc.reserved = reserved;

    __asm__ __volatile__("invept %[desc], %[type]\n\t"
                         "setna %[fail]"
                         : [fail] "=q"(fail)
                         : [desc] "m"(desc), [type] "r"((u64)type)
                         : "cc", "memory");
    return fail ? EINVAL : 0;
}

void vmx_ept_invalidate(struct vmx_vm* vm)
{
    int retval;

    if (vmx_state.ept_vpid_cap & EPT_CAP_INVEPT_SINGLE)
        retval = invept(1, vm->eptp, 0);
    else
        retval = invept(2, 0, 0);

    if (retval) panic("vmx: INVEPT failed");
}

int vmx_ept_init_vm(struct vmx_vm* vm)
{
    s32 pml4 = ept_alloc_page();
    if (pml4 < 0) return ENOMEM;

    u64 pa = __pa(vmx_ept_pool[pml4]);

    /* WB memory type (6), 4-level walks (3), no A/D bits */
    vm->eptp = pa | (6ULL) | (3ULL << 3);
    return 0;
}

static u64* ept_walk(u64 table_phys, u64 gpa, int create, int* err)
{
    u64* table = __va(table_phys);
    int shift;

    *err = 0;
    for (shift = 39; shift > 12; shift -= 9) {
        int idx = (gpa >> shift) & (EPT_ENTRIES - 1);
        u64 entry = table[idx];

        if (!(entry & EPT_PERM(7))) {
            s32 page;
            if (!create) return NULL;
            page = ept_alloc_page();
            if (page < 0) {
                *err = ENOMEM;
                return NULL;
            }
            entry = __pa(vmx_ept_pool[page]) | EPT_PERM(7);
            table[idx] = entry;
        }

        table = __va(entry & EPT_ADDR_MASK);
    }

    return &table[(gpa >> 12) & (EPT_ENTRIES - 1)];
}

int vmx_ept_map_page(struct vmx_vm* vm, u64 gpa, u64 pa, u32 flags)
{
    int err;
    u64* entry = ept_walk(vm->eptp & EPT_ADDR_MASK, gpa, 1, &err);

    if (!entry) return err ? err : EINVAL;
    if (!flags) return EINVAL;

    *entry = (pa & EPT_ADDR_MASK) | EPT_PERM(flags);
    return 0;
}

static void ept_free_table(u64 table_phys, int level)
{
    u64* table = __va(table_phys);
    int i;

    if (level > 0) {
        for (i = 0; i < EPT_ENTRIES; i++) {
            u64 entry = table[i];
            if (entry & EPT_PERM(7)) {
                ept_free_table(entry & EPT_ADDR_MASK, level - 1);
            }
        }
    }

    ept_free_page((__va(table_phys) - (void*)vmx_ept_pool) / ARCH_PG_SIZE);
}

void vmx_ept_destroy_vm(struct vmx_vm* vm)
{
    u64 root = vm->eptp & EPT_ADDR_MASK;

    /* flush cached EPT translations before the pages are reused */
    if (root) {
        vmx_ept_invalidate(vm);
        ept_free_table(root, 3);
    }

    vm->eptp = 0;
}
