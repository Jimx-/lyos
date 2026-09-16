/* AMD nested-page-table management. */
#include <lyos/types.h>
#include <errno.h>
#include <string.h>
#include <asm/page.h>
#include <lyos/bitmap.h>
#include "svm.h"

#define NPT_POOL_WORDS BITCHUNKS(SVM_NPT_POOL_PAGES)
#define NPT_ADDR_MASK  0x000ffffffffff000ULL
#define NPT_ENTRIES    512
#define NPT_PRESENT    (1ULL << 0)
#define NPT_WRITE      (1ULL << 1)
#define NPT_USER       (1ULL << 2)
#define NPT_NX         (1ULL << 63)

static u8 npt_pool[SVM_NPT_POOL_PAGES][ARCH_PG_SIZE]
    __attribute__((aligned(ARCH_PG_SIZE)));
static bitchunk_t npt_pool_map[NPT_POOL_WORDS];
static u32 npt_pages_used;

static s32 npt_alloc_page(void)
{
    int i;
    for (i = 0; i < SVM_NPT_POOL_PAGES; i++) {
        if (!GET_BIT(npt_pool_map, i)) {
            SET_BIT(npt_pool_map, i);
            npt_pages_used++;
            memset(npt_pool[i], 0, ARCH_PG_SIZE);
            return i;
        }
    }
    return -1;
}

static void npt_free_page(int i)
{
    if (i < 0 || i >= SVM_NPT_POOL_PAGES) return;
    UNSET_BIT(npt_pool_map, i);
    if (npt_pages_used) npt_pages_used--;
}

u32 svm_npt_pages_in_use(void) { return npt_pages_used; }

int svm_npt_init_vm(struct svm_vm* vm)
{
    s32 root = npt_alloc_page();
    if (root < 0) return ENOMEM;
    vm->ncr3 = __pa(npt_pool[root]);
    return 0;
}

static u64* npt_walk(u64 table_phys, u64 gpa, int create, int* err)
{
    u64* table = __va(table_phys);
    int shift;

    *err = 0;
    for (shift = 39; shift > 12; shift -= 9) {
        unsigned idx = (gpa >> shift) & (NPT_ENTRIES - 1);
        u64 entry = table[idx];
        if (!(entry & NPT_PRESENT)) {
            s32 page;
            if (!create) return NULL;
            page = npt_alloc_page();
            if (page < 0) {
                *err = ENOMEM;
                return NULL;
            }
            entry = __pa(npt_pool[page]) | NPT_PRESENT | NPT_WRITE | NPT_USER;
            table[idx] = entry;
        }
        table = __va(entry & NPT_ADDR_MASK);
    }
    return &table[(gpa >> 12) & (NPT_ENTRIES - 1)];
}

int svm_npt_map_page(struct svm_vm* vm, u64 gpa, u64 pa, u32 flags)
{
    int err;
    u64 entry = (pa & NPT_ADDR_MASK) | NPT_PRESENT | NPT_USER;
    u64* pte = npt_walk(vm->ncr3, gpa, 1, &err);

    if (!pte) return err ? err : EINVAL;
    if (!flags) return EINVAL;
    if (flags & HV_MEM_WRITE) entry |= NPT_WRITE;
    if (!(flags & HV_MEM_EXEC)) entry |= NPT_NX;
    *pte = entry;
    return 0;
}

void svm_npt_invalidate(struct svm_vm* vm)
{
    if (vm->vcpu) {
        struct svm_vmcb* vmcb = __va(vm->vcpu->vmcb_phys);
        vmcb->control.tlb_ctl = 1; /* flush all entries for this ASID */
        vmcb->control.clean = 0;
    }
}

static void npt_free_table(u64 table_phys, int level)
{
    u64* table = __va(table_phys);
    int i;

    if (level > 0) {
        for (i = 0; i < NPT_ENTRIES; i++) {
            if (table[i] & NPT_PRESENT)
                npt_free_table(table[i] & NPT_ADDR_MASK, level - 1);
        }
    }
    npt_free_page((__va(table_phys) - (void*)npt_pool) / ARCH_PG_SIZE);
}

void svm_npt_destroy_vm(struct svm_vm* vm)
{
    if (vm->ncr3) npt_free_table(vm->ncr3, 3);
    vm->ncr3 = 0;
}
