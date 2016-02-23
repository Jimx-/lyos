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

#include <lyos/compile.h>
#include <lyos/type.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/ipc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/list.h>
#include <sys/futex.h>
    
#include "global.h"
#include "proto.h"
#include "futex.h"

#define QUEUE_HASH_LOG2   7    
#define QUEUE_HASH_SIZE   ((unsigned long)1<<QUEUE_HASH_LOG2)
#define QUEUE_HASH_MASK   (((unsigned long)1<<QUEUE_HASH_LOG2)-1)

PRIVATE struct list_head futex_queues[QUEUE_HASH_SIZE];

PUBLIC void futex_init()
{
    int i;
    for (i = 0; i < QUEUE_HASH_MASK; i++) {
        INIT_LIST_HEAD(&futex_queues[i]);
    }
}

PRIVATE struct list_head* futex_hash(union futex_key* key)
{
    u32 hash = key->both.word + key->both.offset;

    return &futex_queues[hash & QUEUE_HASH_MASK];
}

PRIVATE inline int futex_match_key(union futex_key* k1, union futex_key* k2)
{
    return (k1 && k2 && k1->both.word == k2->both.word && k1->both.ptr == k2->both.ptr && k1->both.offset == k2->both.offset);
}

PRIVATE int futex_get_key(struct mmproc* mmp, u32* uaddr, int shared, union futex_key* key)
{
    /* set the parameters properly in key */
    unsigned long addr = (unsigned long) uaddr;
    struct mm_struct* mm = mmp->active_mm;

    key->both.offset = addr % ARCH_PG_SIZE;
    addr -= key->both.offset;

    if (!shared) {
        key->private.mm = mm;
        key->private.addr = addr;
        return 0;
    }

    return EINVAL;
}

PRIVATE inline void futex_queue(struct mmproc* mmp, struct futex_entry* entry, struct list_head* list)
{
    /* put mmp in the waiting queue */
    INIT_LIST_HEAD(&entry->list);
    list_add(&entry->list, list);
    entry->mmp = mmp;
}

PRIVATE int futex_wait_setup(struct mmproc* mmp, u32* uaddr, unsigned int flags, u32 val,
                       struct futex_entry* entry, struct list_head** list)
{
    int ret;

    ret = futex_get_key(mmp, uaddr, 0, &entry->key);
    if (ret) {
        return ret;
    }

    *list = futex_hash(&entry->key);

    /* map uaddr in current address space */
    off_t offset = (vir_bytes) uaddr % ARCH_PG_SIZE;
    uaddr = (u32*)((vir_bytes) uaddr - offset);
    phys_bytes phys_addr = pgd_va2pa(&mmp->active_mm->pgd, (vir_bytes) uaddr);
    if (!phys_addr) {
        return EFAULT;
    }
    vir_bytes vaddr = alloc_vmpages(1);
    if (!vaddr) return ENOMEM;

    pt_writemap(&mmproc_table[TASK_MM].mm->pgd, (void *)phys_addr, (void *)vaddr, ARCH_PG_SIZE, ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER);
    u32 uval = *(u32*)(vaddr + offset);

    if (uval != val) {
        ret = EAGAIN;
    }

    free_vmpages(vaddr, 1);

    return ret;
}

PRIVATE int futex_wait(struct mmproc* mmp, u32* uaddr, unsigned int flags, u32 val,
                       u64 abs_time, u32 bitset)
{
    int ret;
    struct futex_entry* q = &mmp->futex_entry;
    struct list_head* list;

    if (!bitset) return EINVAL;
    q->bitset = bitset;

    ret = futex_wait_setup(mmp, uaddr, flags, val, q, &list);
    if (ret) return ret;

    futex_queue(mmp, q, list);

    return SUSPEND;
}

PRIVATE void wakeup_proc(struct mmproc* mmp)
{
    MESSAGE msg;
    msg.type = SYSCALL_RET;
    msg.RETVAL = 0;
    send_recv(SEND_NONBLOCK, mmp->endpoint, &msg);
}

PRIVATE int futex_wake(struct mmproc* mmp, u32* uaddr, unsigned int flags, int nr_wake,
                       u32 bitset)
{
    union futex_key key;
    int ret;
    struct list_head wake_queue;
    INIT_LIST_HEAD(&wake_queue);

    if (!bitset) return EINVAL;

    ret = futex_get_key(mmp, uaddr, 0, &key);
    if (ret) return ret;

    struct list_head* list = futex_hash(&key);
    struct futex_entry *q, *tmp;
    list_for_each_entry_safe(q, tmp, list, list) {
        if (futex_match_key(&q->key, &key)) {
            if (!(q->bitset & bitset)) continue;

            list_del(&q->list);
            list_add(&q->list, &wake_queue);

            if (++ret >= nr_wake) break;
        }
    }

    list_for_each_entry_safe(q, tmp, &wake_queue, list) {
        wakeup_proc(q->mmp);
        list_del(&q->list);
    }

    return ret;
}

PUBLIC int do_futex()
{
    int op = mm_msg.FUTEX_OP;
    u32* uaddr = (u32*)mm_msg.FUTEX_UADDR;
    u32 val = (u32)mm_msg.FUTEX_VAL;
    u32 val3 = (u32)mm_msg.FUTEX_VAL3;
    struct mmproc* mmp = endpt_mmproc(mm_msg.source);

    unsigned int flags = 0;

    switch (op) {
    case FUTEX_WAIT:
        val3 = FUTEX_BITSET_MATCH_ANY;
        return futex_wait(mmp, uaddr, flags, val, 0, val3);
    case FUTEX_WAKE:
        val3 = FUTEX_BITSET_MATCH_ANY;
        return futex_wake(mmp, uaddr, flags, val, val3);
    }

    return ENOSYS;
}
