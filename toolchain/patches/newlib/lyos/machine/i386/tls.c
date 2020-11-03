#include <sys/tls.h>
#include <asm/ldt.h>

extern int set_thread_area(struct user_desc* desc);

int __libc_set_tls_tcb(struct tls_tcb* ptr)
{
    struct user_desc desc;
    int sel, retval;

    desc.entry_number = -1;
    desc.base_addr = (unsigned int)ptr;

    retval = set_thread_area(&desc);

    if (retval == 0) {
        sel = (desc.entry_number << 3) | 3;
        asm __volatile__("movw %w0, %%gs" ::"q"(sel));
    }

    return retval;
}
