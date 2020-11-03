#ifndef _I386_TLS_H_
#define _I386_TLS_H_

#define __HAVE_TLS_VARIANT_2

#define __libc_get_tls_tcb()                      \
    ({                                            \
        struct tls_tcb* __val;                    \
        __asm__("movl %%gs:0, %0" : "=r"(__val)); \
        __val;                                    \
    })

#endif
