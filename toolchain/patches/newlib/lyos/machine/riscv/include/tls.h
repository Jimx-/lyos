#ifndef _RISCV_TLS_H_
#define _RISCV_TLS_H_

#define __HAVE_TLS_VARIANT_1

#define __libc_get_tls_tcb()                                         \
    ({                                                               \
        struct tls_tcb* __val;                                       \
        __asm __volatile("addi %[__val],tp,%[__offset]"              \
                         : [__val] "=r"(__val)                       \
                         : [__offset] "n"(-sizeof(struct tls_tcb))); \
        __val;                                                       \
    })

#endif
