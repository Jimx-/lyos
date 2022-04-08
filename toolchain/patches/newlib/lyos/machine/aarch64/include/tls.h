#ifndef _AARCH64_TLS_H_
#define _AARCH64_TLS_H_

#define __HAVE_TLS_VARIANT_1

#define __libc_get_tls_tcb()                                  \
    ({                                                        \
        struct tls_tcb* __val;                                \
        __asm __volatile("mrs\t%0, tpidr_el0" : "=r"(__val)); \
        __val;                                                \
    })

#endif
