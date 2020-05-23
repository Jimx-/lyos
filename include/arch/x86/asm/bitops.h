#ifndef _ARCH_BITOPS_H_
#define _ARCH_BITOPS_H_

static inline __attribute__((always_inline)) int
arch_test_and_clear_bit_non_atomic(long nr, volatile unsigned long* addr)
{
    int oldbit;

    asm volatile("btrl %2,%1\n"
                 : "=@ccc"(oldbit)
                 : "m"(*(volatile long*)addr), "Ir"(nr)
                 : "memory");
    return oldbit;
}

static inline __attribute__((always_inline)) int
arch_test_and_set_bit_non_atomic(long nr, volatile unsigned long* addr)
{
    int oldbit;

    asm volatile("btsl %2,%1\n"
                 : "=@ccc"(oldbit)
                 : "m"(*(volatile long*)addr), "Ir"(nr)
                 : "memory");
    return oldbit;
}

#endif
