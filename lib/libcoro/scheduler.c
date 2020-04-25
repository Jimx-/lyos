#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <libcoro/libcoro.h>

#include "coro_internal.h"
#include "global.h"
#include "proto.h"

void coro_scheduler_init(void) { coro_queue_init(&run_queue); }

void coro_schedule(void)
{
    coro_thread_t old_thread;
    coro_tcb_t *new_tcb, *old_tcb;

    old_thread = current_thread;

    if (coro_queue_empty(&run_queue)) {
        if (old_thread == MAIN_THREAD) {
            return;
        }

        current_thread = MAIN_THREAD;
    } else {
        current_thread = coro_queue_dequeue(&run_queue);
    }

    new_tcb = coro_find_tcb(current_thread);
    old_tcb = coro_find_tcb(old_thread);

    if (swapcontext(&old_tcb->context, &new_tcb->context) == -1) {
        panic("failed to swap context");
    }
}

int coro_yield(void)
{
    if (coro_queue_empty(&run_queue) || current_thread == NO_THREAD) {
        return -1;
    }

    coro_queue_enqueue(&run_queue, current_thread);
    coro_suspend(CR_RUNNABLE);

    return 0;
}

void coro_suspend(coro_state_t state)
{
    coro_tcb_t* tcb = coro_find_tcb(current_thread);
    tcb->state = state;

    coro_schedule();
}

void coro_unsuspend(coro_thread_t thread)
{
    coro_tcb_t* tcb = coro_find_tcb(thread);
    tcb->state = CR_RUNNABLE;
    coro_queue_enqueue(&run_queue, thread);
}
