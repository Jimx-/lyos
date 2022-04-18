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
#include <lyos/const.h>
#include <lyos/list.h>
#include <lyos/sysutils.h>
#include <sys/futex.h>
#include <asm/page.h>

#include "proto.h"
#include "futex.h"
#include "pmproc.h"

#define QUEUE_HASH_LOG2 7
#define QUEUE_HASH_SIZE ((unsigned long)1 << QUEUE_HASH_LOG2)
#define QUEUE_HASH_MASK (((unsigned long)1 << QUEUE_HASH_LOG2) - 1)

static struct list_head futex_queues[QUEUE_HASH_SIZE];

void futex_init(void)
{
    int i;
    for (i = 0; i < QUEUE_HASH_MASK; i++) {
        INIT_LIST_HEAD(&futex_queues[i]);
    }
}

static struct list_head* futex_hash(union futex_key* key)
{
    u32 hash = key->both.word + key->both.offset;

    return &futex_queues[hash & QUEUE_HASH_MASK];
}

static inline int futex_match_key(union futex_key* k1, union futex_key* k2)
{
    return (k1 && k2 && k1->both.word == k2->both.word &&
            k1->both.ptr == k2->both.ptr && k1->both.offset == k2->both.offset);
}

static int futex_get_key(struct pmproc* pmp, u32* uaddr, int shared,
                         union futex_key* key)
{
    /* set the parameters properly in key */
    unsigned long addr = (unsigned long)uaddr;

    key->both.offset = addr % ARCH_PG_SIZE;
    addr -= key->both.offset;

    if (!shared) {
        key->private.pmp = pmp->group_leader;
        key->private.addr = addr;
        return 0;
    }

    return EINVAL;
}

static inline void futex_queue(struct pmproc* pmp, struct futex_entry* entry,
                               struct list_head* list)
{
    /* put mmp in the waiting queue */
    INIT_LIST_HEAD(&entry->list);
    list_add(&entry->list, list);
    entry->pmp = pmp;
}

static int futex_wait_setup(struct pmproc* pmproc, u32* uaddr,
                            unsigned int flags, u32 val,
                            struct futex_entry* entry, struct list_head** list)
{
    int ret;
    u32 uval;

    ret = futex_get_key(pmproc, uaddr, 0, &entry->key);
    if (ret) {
        return ret;
    }

    *list = futex_hash(&entry->key);

    ret = data_copy(SELF, &uval, pmproc->endpoint, uaddr, sizeof(uval));
    if (ret) {
        return ret;
    }

    if (uval != val) {
        return EAGAIN;
    }

    return 0;
}

static int futex_wait(struct pmproc* pmp, u32* uaddr, unsigned int flags,
                      u32 val, u64 abs_time, u32 bitset)
{
    int ret;
    struct futex_entry* q = &pmp->futex_entry;
    struct list_head* list;

    if (!bitset) return EINVAL;
    q->bitset = bitset;

    ret = futex_wait_setup(pmp, uaddr, flags, val, q, &list);
    if (ret) return ret;

    futex_queue(pmp, q, list);

    return SUSPEND;
}

static void wakeup_proc(struct pmproc* pmp)
{
    MESSAGE msg;

    msg.type = SYSCALL_RET;
    msg.RETVAL = 0;
    send_recv(SEND_NONBLOCK, pmp->endpoint, &msg);
}

int futex_wake(struct pmproc* pmp, u32* uaddr, unsigned int flags, int nr_wake,
               u32 bitset)
{
    union futex_key key;
    int ret;
    struct list_head wake_queue;
    INIT_LIST_HEAD(&wake_queue);

    if (!bitset) return EINVAL;

    ret = futex_get_key(pmp, uaddr, 0, &key);
    if (ret) return ret;

    struct list_head* list = futex_hash(&key);
    struct futex_entry *q, *tmp;
    list_for_each_entry_safe(q, tmp, list, list)
    {
        if (futex_match_key(&q->key, &key)) {
            if (!(q->bitset & bitset)) continue;

            list_del(&q->list);
            list_add(&q->list, &wake_queue);

            if (++ret >= nr_wake) break;
        }
    }

    list_for_each_entry_safe(q, tmp, &wake_queue, list)
    {
        wakeup_proc(q->pmp);
        list_del(&q->list);
    }

    return ret;
}

int do_futex(MESSAGE* m)
{
    struct pmproc* pmp = pm_endpt_proc(m->source);
    int op = m->FUTEX_OP;
    u32* uaddr = (u32*)m->FUTEX_UADDR;
    u32 val = (u32)m->FUTEX_VAL;
    u32 val3 = (u32)m->FUTEX_VAL3;

    unsigned int flags = 0;

    switch (op) {
    case FUTEX_WAIT:
        val3 = FUTEX_BITSET_MATCH_ANY;
        return futex_wait(pmp, uaddr, flags, val, 0, val3);
    case FUTEX_WAKE:
        val3 = FUTEX_BITSET_MATCH_ANY;
        return futex_wake(pmp, uaddr, flags, val, val3);
    }

    return ENOSYS;
}
