#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <lyos/list.h>

#define MQ_SIZE 128

struct mq_entry {
    struct list_head list;

    MESSAGE msg;
};

static struct mq_entry mq[MQ_SIZE];
static struct list_head queue;
static struct list_head free_list;

void mq_init(void)
{
    struct mq_entry* mqe;

    INIT_LIST_HEAD(&queue);
    INIT_LIST_HEAD(&free_list);

    for (mqe = mq; mqe < &mq[MQ_SIZE]; mqe++) {
        INIT_LIST_HEAD(&mqe->list);
        list_add(&mqe->list, &free_list);
    }
}

int mq_enqueue(const MESSAGE* msg)
{
    struct mq_entry* mqe;

    if (list_empty(&free_list)) {
        return FALSE;
    }

    mqe = list_entry(free_list.next, struct mq_entry, list);
    list_del(&mqe->list);

    mqe->msg = *msg;

    list_add(&mqe->list, &queue);

    return TRUE;
}

int mq_empty(void) { return list_empty(&queue); }

int mq_dequeue(MESSAGE* msg)
{
    struct mq_entry* mqe;

    if (mq_empty()) return FALSE;

    mqe = list_entry(queue.prev, struct mq_entry, list);
    list_del(&mqe->list);

    *msg = mqe->msg;

    list_add(&mqe->list, &free_list);

    return TRUE;
}
