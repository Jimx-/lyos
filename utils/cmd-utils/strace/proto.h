#ifndef _STRACE_PROTO_H_
#define _STRACE_PROTO_H_

#include "types.h"
#include "xlat.h"

const char* xlookup(const struct xlat* xlat, uint64_t val);

void print_path(struct tcb* tcp, char* string, int len);
void print_str(struct tcb* tcp, char* str, int len);
void print_err(int err);

int print_flags(uint64_t flags, const struct xlat* xlat);
const char* sprint_flags(uint64_t flags, const struct xlat* xlat);

void print_addr(const uint64_t addr);
void print_dev_t(uint64_t dev);
void print_mode_t(unsigned int mode);

typedef int (*print_fn)(struct tcb*, void*, size_t, void*);
typedef int (*fetch_mem_fn)(struct tcb*, const void*, void*, size_t);

int fetch_mem(struct tcb* tcp, const void* addr, void* buf, size_t size);
int print_array(struct tcb* tcp, const void* start, size_t count,
                void* elem_buf, size_t elem_size, fetch_mem_fn fetch_fn,
                print_fn print_elem_fn, void* arg);

int decode_sockaddr(struct tcb* tcp, const void* addr, size_t addrlen);

const char* sprint_signame(int sig);
int trace_sigprocmask(struct tcb* tcp);
int trace_signalfd(struct tcb* tcp);
int trace_kill(struct tcb* tcp);

int trace_stat(struct tcb* tcp);
int trace_fstat(struct tcb* tcp);

int trace_brk(struct tcb* tcp);
int trace_mmap(struct tcb* tcp);
int trace_munmap(struct tcb* tcp);

int trace_dup(struct tcb* tcp);

int trace_ioctl(struct tcb* tcp);

int trace_open(struct tcb* tcp);

int trace_unlink(struct tcb* tcp);

int trace_pipe2(struct tcb* tcp);
int trace_socket(struct tcb* tcp);
int trace_bind(struct tcb* tcp);
int trace_connect(struct tcb* tcp);
int trace_listen(struct tcb* tcp);
int trace_accept(struct tcb* tcp);

int trace_fork(struct tcb* tcp);

int trace_waitpid(struct tcb* tcp);

int trace_poll(struct tcb* tcp);

int trace_eventfd(struct tcb* tcp);

int trace_timerfd_create(struct tcb* tcp);
int trace_timerfd_settime(struct tcb* tcp);
int trace_timerfd_gettime(struct tcb* tcp);

int trace_epoll_create1(struct tcb* tcp);
int trace_epoll_ctl(struct tcb* tcp);
int trace_epoll_wait(struct tcb* tcp);

#endif
