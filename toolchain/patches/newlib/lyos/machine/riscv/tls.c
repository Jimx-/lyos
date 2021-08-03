#include <sys/tls.h>

int __libc_set_tls_tcb(struct tls_tcb* __ptr)
{
    __asm __volatile(
        "addi tp,%[__ptr],%[__offset]"
        :
        : [__ptr] "r"(__ptr), [__offset] "n"(sizeof(struct tls_tcb)));
}
