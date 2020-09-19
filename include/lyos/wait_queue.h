#ifndef _WAIT_QUEUE_H_
#define _WAIT_QUEUE_H_

#include <lyos/list.h>

struct wait_queue_entry;

typedef int (*wait_queue_func_t)(struct wait_queue_entry* wq_entry, void* arg);

struct wait_queue_entry {
    void* private;
    wait_queue_func_t func;
    struct list_head entry;
};

struct wait_queue_head {
    struct list_head head;
};

static inline void init_waitqueue_head(struct wait_queue_head* head)
{
    INIT_LIST_HEAD(&head->head);
}

static inline void init_waitqueue_entry_func(struct wait_queue_entry* entry,
                                             wait_queue_func_t func)
{
    entry->private = NULL;
    entry->func = func;
}

static inline void waitqueue_add(struct wait_queue_head* head,
                                 struct wait_queue_entry* entry)
{
    list_add(&entry->entry, &head->head);
}

static inline void waitqueue_remove(struct wait_queue_head* head,
                                    struct wait_queue_entry* entry)
{
    list_del(&entry->entry);
}

static inline void waitqueue_wakeup_all(struct wait_queue_head* head, void* arg)
{
    struct wait_queue_entry *curr, *next;
    int retval;

    list_for_each_entry_safe(curr, next, &head->head, entry)
    {
        retval = curr->func(curr, arg);
        if (retval < 0) break;
    }
}

#endif
