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
#include <lyos/const.h>
#include <lyos/bitmap.h>
#include <errno.h>
#include <signal.h>
#include <kernel/proto.h>
#include <kernel/hypervisor.h>
#include <asm/page.h>

const struct hv_backend_ops* hv_backend;

int hv_backend_register(const struct hv_backend_ops* ops)
{
    if (hv_backend) return EBUSY;
    if (!ops || ops->backend == HV_BACKEND_NONE || !ops->state_size ||
        ops->state_size > HV_STATE_MAX_SIZE || !ops->exit_size ||
        ops->exit_size > HV_EXIT_MAX_SIZE || !ops->query_caps ||
        !ops->vm_create || !ops->vm_destroy || !ops->vm_set_memslot ||
        !ops->vcpu_create || !ops->vcpu_destroy || !ops->vcpu_set_state ||
        !ops->vcpu_get_state || !ops->vcpu_run || !ops->proc_cleanup)
        return EINVAL;
    hv_backend = ops;
    return 0;
}

void hv_init(void) { arch_hv_init(); }

void hv_proc_cleanup(struct proc* p)
{
    if (hv_backend) hv_backend->proc_cleanup(p);
}

/* Guest RAM backing store and MM release notifications. */

static u64 backing_pool[HV_BACKING_POOL_PAGES];
static bitchunk_t backing_map[BITCHUNKS(HV_BACKING_POOL_PAGES)];
static unsigned long backing_pages_held;
static struct hv_backing backing_table[HV_MAX_BACKINGS];

static u32 release_queue[HV_RELEASE_QUEUE];
static int release_count;

static long backing_alloc(unsigned long n)
{
    unsigned long i, j;

    for (i = 0; i + n <= HV_BACKING_POOL_PAGES; i++) {
        for (j = 0; j < n; j++) {
            if (GET_BIT(backing_map, i + j)) break;
        }
        if (j == n) {
            for (j = 0; j < n; j++)
                SET_BIT(backing_map, i + j);
            return (long)i;
        }
    }
    return -1;
}

static void backing_free(unsigned long off, unsigned long n)
{
    unsigned long j;
    for (j = 0; j < n; j++)
        UNSET_BIT(backing_map, off + j);
}

struct hv_backing* hv_backing_lookup(u32 mm_handle)
{
    int i;
    for (i = 0; i < HV_MAX_BACKINGS; i++) {
        if (backing_table[i].in_use && backing_table[i].mm_handle == mm_handle)
            return &backing_table[i];
    }
    return NULL;
}

static void queue_backing_release(u32 mm_handle)
{
    if (release_count >= HV_RELEASE_QUEUE) {
        printk("hypervisor: release queue full, MM handle %u leaked\n",
               mm_handle);
        return;
    }
    release_queue[release_count++] = mm_handle;

    /* ask MM to drain the release queue */
    send_sig(TASK_MM, SIGKMEM);
}

static void release_backing(struct hv_backing* b)
{
    backing_free(b->pool_off, b->npages);
    backing_pages_held -= b->npages;
    queue_backing_release(b->mm_handle);
    b->in_use = 0;
}

int hv_backing_register(u32 mm_handle, unsigned long npages, const u64* pages)
{
    struct hv_backing* b = NULL;
    unsigned long i;
    long off;

    if (!hv_backend) return EIO;
    if (npages == 0 || npages > HV_BACKING_POOL_PAGES) return EINVAL;
    if (hv_backing_lookup(mm_handle)) return EBUSY;

    {
        int j;
        for (j = 0; j < HV_MAX_BACKINGS; j++) {
            if (!backing_table[j].in_use) {
                b = &backing_table[j];
                break;
            }
        }
    }
    if (!b) return ENOSPC;

    off = backing_alloc(npages);
    if (off < 0) return ENOMEM;

    for (i = 0; i < npages; i++) {
        if (pages[i] & (ARCH_PG_SIZE - 1)) {
            backing_free(off, npages);
            return EINVAL;
        }
        backing_pool[off + i] = pages[i];
    }

    b->in_use = 1;
    b->mm_handle = mm_handle;
    b->npages = npages;
    b->pool_off = off;
    b->refs = 0;
    backing_pages_held += npages;

    return 0;
}

int hv_backing_unregister(u32 mm_handle)
{
    struct hv_backing* b = hv_backing_lookup(mm_handle);

    if (!b) return ENOENT;
    if (b->refs) return EBUSY;
    release_backing(b);
    return 0;
}

int hv_backing_release_get(u32* mm_handle)
{
    if (!release_count) {
        *mm_handle = 0;
        return 0;
    }
    *mm_handle = release_queue[--release_count];
    return 0;
}

void hv_backing_get(struct hv_backing* backing) { backing->refs++; }

void hv_backing_put(struct hv_backing* backing)
{
    if (!backing->refs) panic("hypervisor: backing reference underflow");
    if (--backing->refs == 0) release_backing(backing);
}

u64 hv_backing_page(const struct hv_backing* backing, unsigned long index)
{
    if (index >= backing->npages)
        panic("hypervisor: backing page out of range");
    return backing_pool[backing->pool_off + index];
}

void hv_backing_query_caps(struct hv_caps* caps)
{
    int i;
    caps->backing_pages = backing_pages_held;
    caps->backings = 0;
    for (i = 0; i < HV_MAX_BACKINGS; i++)
        if (backing_table[i].in_use) caps->backings++;
}
