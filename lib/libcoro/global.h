#ifndef _LIBCORO_GLOBAL_H_
#define _LIBCORO_GLOBAL_H_

#include <lyos/type.h>

/* EXTERN is extern except for global.c */
#ifdef _LIBCORO_GLOBAL_VARIABLE_HERE_
#undef EXTERN
#define EXTERN
#endif

#define MAIN_THREAD ((coro_thread_t)-1)
#define NO_THREAD ((coro_thread_t)-2)
#define NR_THREADS 4
#define MAX_THREADS 1024
#define is_valid_id(id) ((id == MAIN_THREAD) || (id >= 0 && id < nr_threads))

EXTERN coro_thread_t current_thread;
EXTERN coro_queue_t free_threads;
EXTERN coro_queue_t run_queue;
EXTERN coro_tcb_t main_thread;
EXTERN coro_tcb_t** threads;
EXTERN int nr_threads;

#endif
