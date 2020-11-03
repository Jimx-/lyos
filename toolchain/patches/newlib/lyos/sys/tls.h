#ifndef _SYS_TLS_H_
#define _SYS_TLS_H_

#include <sys/cdefs.h>
#include <machine/tls.h>

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)

#if defined(__HAVE_TLS_VARIANT_1) && defined(__HAVE_TLS_VARIANT_2)
#error Only one TLS variant can be supported at a time
#endif

struct tls_tcb {
    void* tcb_self;
    void* tcb_pthread;
};

__BEGIN_DECLS

int __libc_set_tls_tcb(struct tls_tcb* ptr);

__END_DECLS

#endif /* defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2) */

#endif /* _SYS_TLS_H_ */
