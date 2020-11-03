#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include "ldso.h"

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)

size_t ldso_tls_static_space;
size_t ldso_tls_static_offset;

size_t ldso_tls_dtv_generation = 1;
size_t ldso_tls_max_index = 1;

int ldso_tls_allocate_offset(struct so_info* si)
{
    size_t offset, next_offset;

    if (si->tls_done) return 0;
    if (si->tls_size == 0) {
        si->tls_offset = 0;
        si->tls_done = 1;
        return 0;
    }

#ifdef __HAVE_TLS_VARIANT_1

#else
    offset = ldso_tls_static_offset + si->tls_size;
    if (offset % si->tls_align)
        offset += si->tls_align - (offset % si->tls_align);

    next_offset = offset;
#endif

    si->tls_offset = offset;
    ldso_tls_static_offset = next_offset;
    si->tls_done = 1;

    return 0;
}

static struct tls_tcb* ldso_allocate_tls_locked(void)
{
    struct so_info* si;
    struct tls_tcb* tcb;
    char *p, *q;

    p = mmap(NULL, ldso_tls_static_space + sizeof(struct tls_tcb),
             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;

#ifdef __HAVE_TLS_VARIANT_1

#else
    p += ldso_tls_static_space;
    tcb = (struct tls_tcb*)p;
    tcb->tcb_self = tcb;
#endif

    for (si = si_list; si != NULL; si = si->next) {
        if (si->tls_init_size && si->tls_done) {
#ifdef __HAVE_TLS_VARIANT_1

#else
            q = p - si->tls_offset;
#endif
            /* xprintf("TLS: dtv %p in %p -> tls offset %u\n", q, si, */
            /*         si->tls_offset); */
            memcpy(q, si->tls_init, si->tls_init_size);
        }
    }

    return tcb;
}

void ldso_tls_initial_allocation(void)
{
    struct tls_tcb* tcb;

    ldso_tls_static_space =
        ldso_tls_static_offset + LDSO_STATIC_TLS_RESERVATION;

    tcb = ldso_allocate_tls_locked();

    __libc_set_tls_tcb(tcb);
}

struct tls_tcb* __ldso_allocate_tls(void) { return ldso_allocate_tls_locked(); }

#endif /* defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2) */
