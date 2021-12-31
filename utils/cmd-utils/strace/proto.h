#ifndef _STRACE_PROTO_H_
#define _STRACE_PROTO_H_

#include <time.h>

#include "types.h"
#include "xlat.h"

extern size_t max_strlen;

const char* xlookup(const struct xlat* xlat, uint64_t val);

void print_path(struct tcb* tcp, char* string, int len);

/** String is '\0'-terminated. */
#define QUOTE_0_TERMINATED 0x01
/** Do not emit leading and ending '"' characters. */
#define QUOTE_OMIT_LEADING_TRAILING_QUOTES 0x02
/** Do not print '\0' if it is the last character. */
#define QUOTE_OMIT_TRAILING_0 0x08
/** Print ellipsis if the last character is not '\0' */
#define QUOTE_EXPECT_TRAILING_0 0x10
/** Print string in hex (using '\xHH' notation). */
#define QUOTE_FORCE_HEX 0x20
/** Enclose the string in C comment syntax. */
#define QUOTE_EMIT_COMMENT 0x40
int string_quote(const char* instr, char* outstr, size_t size,
                 unsigned int style, const char* escape_chars);
int print_str(struct tcb* tcp, const void* addr, size_t len,
              unsigned int user_style);

static inline int print_strn(struct tcb* tcp, const void* str, size_t len)
{
    return print_str(tcp, str, len, 0);
}

void print_err(int err);

int print_xval(uint64_t val, const struct xlat* xlat);
int print_flags(uint64_t flags, const struct xlat* xlat);
const char* sprint_flags(uint64_t flags, const struct xlat* xlat);

void print_addr(const uint64_t addr);
void print_dev_t(uint64_t dev);
void print_mode_t(unsigned int mode);
void print_dirfd(int fd);

typedef int (*print_fn)(struct tcb*, void*, size_t, void*);
typedef int (*fetch_mem_fn)(struct tcb*, const void*, void*, size_t);

int fetch_mem(struct tcb* tcp, const void* addr, void* buf, size_t size);
int print_array(struct tcb* tcp, const void* start, size_t count,
                void* elem_buf, size_t elem_size, fetch_mem_fn fetch_fn,
                print_fn print_elem_fn, void* arg);

void print_iov_upto(struct tcb* const tcp, size_t len, void* addr,
                    unsigned long data_size);

void print_timespec_t(const struct timespec* ts);

int syscall_trace_entering(struct tcb* tcp, int* sig);
int syscall_trace_exiting(struct tcb* tcp);

int decode_sockaddr(struct tcb* tcp, const void* addr, size_t addrlen);

const char* sprint_signame(int sig);
int trace_sigprocmask(struct tcb* tcp);
int trace_signalfd(struct tcb* tcp);
int trace_kill(struct tcb* tcp);

int trace_stat(struct tcb* tcp);
int trace_lstat(struct tcb* tcp);
int trace_fstat(struct tcb* tcp);
int trace_fstatat(struct tcb* tcp);

int trace_fcntl(struct tcb* tcp);

int trace_access(struct tcb* tcp);

int trace_brk(struct tcb* tcp);
int trace_mmap(struct tcb* tcp);
int trace_munmap(struct tcb* tcp);
int trace_mremap(struct tcb* tcp);

int trace_dup(struct tcb* tcp);

int trace_ioctl(struct tcb* tcp);

int trace_open(struct tcb* tcp);
int trace_openat(struct tcb* tcp);
int trace_close(struct tcb* tcp);
int trace_read(struct tcb* tcp);
int trace_write(struct tcb* tcp);

int trace_exec(struct tcb* tcp);

int trace_getsetid(struct tcb* tcp);

int trace_chmod(struct tcb* tcp);
int trace_umask(struct tcb* tcp);

int trace_fchown(struct tcb* tcp);
int trace_fchownat(struct tcb* tcp);

int trace_getdents(struct tcb* tcp);

int trace_symlink(struct tcb* tcp);
int trace_unlink(struct tcb* tcp);
int trace_unlinkat(struct tcb* tcp);
int trace_readlinkat(struct tcb* tcp);

int trace_pipe2(struct tcb* tcp);
int trace_socket(struct tcb* tcp);
int trace_socketpair(struct tcb* tcp);
int trace_bind(struct tcb* tcp);
int trace_connect(struct tcb* tcp);
int trace_listen(struct tcb* tcp);
int trace_accept4(struct tcb* tcp);
int trace_sendto(struct tcb* tcp);
int trace_recvfrom(struct tcb* tcp);
int trace_getsockname(struct tcb* tcp);

int decode_sockaddr(struct tcb* tcp, const void* addr, size_t addrlen);

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

int trace_sendmsg(struct tcb* tcp);
int trace_recvmsg(struct tcb* tcp);

int trace_utimensat(struct tcb* tcp);

int trace_ftruncate(struct tcb* tcp);

/* Arch-specific */
void arch_get_syscall_args(struct tcb* tcp);
void arch_get_syscall_rval(struct tcb* tcp);

#endif
