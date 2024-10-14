#ifndef _ASM_BARRIER_H_
#define _ASM_BARRIER_H_

#define cmb() __asm__ __volatile__("" ::: "memory")

#define mb()  asm volatile("mfence" ::: "memory")
#define rmb() asm volatile("lfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")

#endif
