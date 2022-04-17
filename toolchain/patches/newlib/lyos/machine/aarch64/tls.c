#include <sys/tls.h>

int __libc_set_tls_tcb(struct tls_tcb* ptr)
{
    __asm __volatile("msr tpidr_el0, %0" : : "r"(ptr));
    return 0;
}
