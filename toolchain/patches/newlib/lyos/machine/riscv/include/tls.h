#ifndef _RISCV_TLS_H_
#define _RISCV_TLS_H_

#define __HAVE_TLS_VARIANT_1

#define __libc_get_tls_tcb()                \
    ({                                      \
        struct tls_tcb* __val;              \
        __asm__("mv %0, tp" : "=r"(__val)); \
        __val;                              \
    })

#endif
