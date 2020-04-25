#ifndef _LIBCORO_PROTO_H_
#define _LIBCORO_PROTO_H_

/* thread.c */
coro_tcb_t* coro_find_tcb(coro_thread_t thread);
void coro_thread_reset(coro_thread_t thread);

/* queue.c */
void coro_queue_init(coro_queue_t* queue);
int coro_queue_empty(coro_queue_t* queue);
void coro_queue_enqueue(coro_queue_t* queue, coro_thread_t thread);
coro_thread_t coro_queue_dequeue(coro_queue_t* queue);

/* scheduler.c */
void coro_schedule(void);
void coro_scheduler_init();
void coro_suspend(coro_state_t state);
void coro_unsuspend(coro_thread_t thread);

#endif
