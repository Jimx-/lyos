#ifndef _ARCH_DIV64_H_
#define _ARCH_DIV64_H_

#ifdef CONFIG_X86_32

#define do_div(n, base)                                \
    ({                                                 \
        u32 __upper, __low, __high, __mod, __base;     \
        __base = (base);                               \
        asm("" : "=a"(__low), "=d"(__high) : "A"(n));  \
        __upper = __high;                              \
        if (__high) {                                  \
            __upper = __high % (__base);               \
            __high = __high / (__base);                \
        }                                              \
        asm("divl %2"                                  \
            : "=a"(__low), "=d"(__mod)                 \
            : "rm"(__base), "0"(__low), "1"(__upper)); \
        asm("" : "=A"(n) : "a"(__low), "d"(__high));   \
        __mod;                                         \
    })

#else

#define do_div(n, base)                   \
    ({                                    \
        uint32_t __base = (base);         \
        uint32_t __rem;                   \
        __rem = ((uint64_t)(n)) % __base; \
        (n) = ((uint64_t)(n)) / __base;   \
        __rem;                            \
    })

#endif

#endif
