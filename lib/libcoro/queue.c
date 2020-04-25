#include <libcoro/libcoro.h>

#include "coro_internal.h"
#include "global.h"
#include "proto.h"

void coro_queue_init(coro_queue_t* queue) { queue->head = queue->tail = NULL; }

int coro_queue_empty(coro_queue_t* queue) { return queue->head == NULL; }

void coro_queue_enqueue(coro_queue_t* queue, coro_thread_t thread)
{
    coro_tcb_t* last = coro_find_tcb(thread);

    if (coro_queue_empty(queue)) {
        queue->head = queue->tail = last;
    } else {
        queue->tail->next = last;
        queue->tail = last;
    }
}

coro_thread_t coro_queue_dequeue(coro_queue_t* queue)
{
    coro_tcb_t* tcb;
    coro_thread_t thread;

    tcb = queue->head;
    if (tcb == NULL) {
        thread = NO_THREAD;
    } else if (tcb == &main_thread) {
        thread = MAIN_THREAD;
    } else {
        thread = tcb->id;
    }

    if (thread != NO_THREAD) {
        if (queue->head == queue->tail) {
            queue->head = queue->tail = NULL;
        } else {
            queue->head = queue->head->next;
        }
    }

    if (tcb != NULL) {
        tcb->next = NULL;
    }

    return thread;
}
