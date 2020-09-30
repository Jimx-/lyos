#ifndef _STRACE_PROTO_H_
#define _STRACE_PROTO_H_

#include "types.h"
#include "xlat.h"

const char* xlookup(const struct xlat* xlat, uint64_t val);

void print_path(struct tcb* tcp, char* string, int len);
static void print_str(struct tcb* tcp, char* str, int len);
void print_err(int err);
int print_flags(uint64_t flags, const struct xlat* xlat);
void print_addr(const uint64_t addr);
void print_dev_t(uint64_t dev);
void print_mode_t(unsigned int mode);

int trace_stat(struct tcb* tcp);
int trace_fstat(struct tcb* tcp);

int trace_brk(struct tcb* tcp);
int trace_mmap(struct tcb* tcp);
int trace_munmap(struct tcb* tcp);

int trace_dup(struct tcb* tcp);

#endif
