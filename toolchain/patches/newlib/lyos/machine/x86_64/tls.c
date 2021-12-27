#include <sys/tls.h>
#include <asm/prctl.h>

extern int arch_prctl(int option, unsigned long arg2);

int __libc_set_tls_tcb(struct tls_tcb* ptr)
{
    return arch_prctl(ARCH_SET_FS, (unsigned long)ptr);
}
