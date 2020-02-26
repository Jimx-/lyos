#ifndef _ARCH_DIV64_H_
#define _ARCH_DIV64_H_

#define do_div(n, base)                                      \
    ({                                                       \
        unsigned long __upper, __low, __high, __mod, __base; \
        __base = (base);                                     \
        asm("" : "=a"(__low), "=d"(__high) : "A"(n));        \
        __upper = __high;                                    \
        if (__high) {                                        \
            __upper = __high % (__base);                     \
            __high = __high / (__base);                      \
        }                                                    \
        asm("divl %2"                                        \
            : "=a"(__low), "=d"(__mod)                       \
            : "rm"(__base), "0"(__low), "1"(__upper));       \
        asm("" : "=A"(n) : "a"(__low), "d"(__high));         \
        __mod;                                               \
    })

#endif
