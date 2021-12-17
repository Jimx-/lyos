#ifndef _ARCH_BITOPS_H_
#define _ARCH_BITOPS_H_

#include <asm/asm.h>

static inline __attribute__((always_inline)) int
arch_test_and_clear_bit_non_atomic(long nr, volatile unsigned long* addr)
{
    int oldbit;

    asm volatile(__ASM_SIZE(btr) " %2,%1\n"
                 : "=@ccc"(oldbit)
                 : "m"(*(volatile long*)addr), "Ir"(nr)
                 : "memory");
    return oldbit;
}

static inline __attribute__((always_inline)) int
arch_test_and_set_bit_non_atomic(long nr, volatile unsigned long* addr)
{
    int oldbit;

    asm volatile(__ASM_SIZE(bts) " %2,%1\n"
                 : "=@ccc"(oldbit)
                 : "m"(*(volatile long*)addr), "Ir"(nr)
                 : "memory");
    return oldbit;
}

#endif
