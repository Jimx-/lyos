#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>

#include "ldso.h"
#include "debug.h"

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)

size_t ldso_tls_static_space;
size_t ldso_tls_static_offset;

size_t ldso_tls_dtv_generation = 1;
size_t ldso_tls_max_index = 1;

#define DTV_GENERATION(dtv)          ((size_t)((dtv)[0]))
#define DTV_MAX_INDEX(dtv)           ((size_t)((dtv)[-1]))
#define SET_DTV_GENERATION(dtv, val) (dtv)[0] = (void*)(size_t)(val)
#define SET_DTV_MAX_INDEX(dtv, val)  (dtv)[-1] = (void*)(size_t)(val)

void* ldso_tls_get_addr(void* tls, size_t idx, size_t offset)
{
    struct tls_tcb* tcb = tls;
    void** dtv;

    dtv = tcb->tcb_dtv;

    if (DTV_GENERATION(dtv) != ldso_tls_dtv_generation) {
    }

    if (!dtv[idx]) {
        dtv[idx] = ldso_tls_module_allocate(idx);
    }

    return (char*)dtv[idx] + offset;
}

void* ldso_tls_module_allocate(size_t idx)
{
    struct so_info* si = NULL;
    void* p;

    for (si = si_list; si != NULL; si = si->next) {
        if (si->tls_index == idx) break;
    }
    if (!si) {
        xprintf("Module for index %d not found", idx);
        ldso_die("ldso_tls_module_allocate");
    }

    p = mmap(NULL, si->tls_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(p, si->tls_init, si->tls_init_size);

    return p;
}

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
    offset = ldso_tls_static_offset;
    if (offset % si->tls_align)
        offset += si->tls_align - (offset % si->tls_align);
    next_offset = offset + si->tls_size;
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
    size_t dtv_size;

    dtv_size = sizeof(*tcb->tcb_dtv) * (2 + ldso_tls_max_index);

    p = mmap(NULL, ldso_tls_static_space + sizeof(struct tls_tcb) + dtv_size,
             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;

#ifdef __HAVE_TLS_VARIANT_1
    tcb = (struct tls_tcb*)p;
    p += sizeof(struct tls_tcb);
    tcb->tcb_dtv = (void**)(p + ldso_tls_static_space);
    ++tcb->tcb_dtv;
#else
    p += ldso_tls_static_space;
    tcb = (struct tls_tcb*)p;
    tcb->tcb_self = tcb;
    tcb->tcb_dtv = (void**)(p + sizeof(struct tls_tcb));
    ++tcb->tcb_dtv;
#endif

    SET_DTV_MAX_INDEX(tcb->tcb_dtv, ldso_tls_max_index);
    SET_DTV_GENERATION(tcb->tcb_dtv, ldso_tls_dtv_generation);

    for (si = si_list; si != NULL; si = si->next) {
        if (si->tls_done) {
#ifdef __HAVE_TLS_VARIANT_1
            q = p + si->tls_offset;
#else
            q = p - si->tls_offset;
#endif
            dbg(("TLS: dtv %p in %p -> tls offset %u\n", q, si,
                 si->tls_offset));
            if (si->tls_init_size) memcpy(q, si->tls_init, si->tls_init_size);
            tcb->tcb_dtv[si->tls_index] = q;
        }
    }

    return tcb;
}

void ldso_tls_initial_allocation(void)
{
    struct tls_tcb* tcb;

    ldso_tls_static_space =
        ldso_tls_static_offset + LDSO_STATIC_TLS_RESERVATION;

#ifndef __HAVE_TLS_VARIANT_1
    if (ldso_tls_static_space % sizeof(void*))
        ldso_tls_static_space +=
            sizeof(void*) - (ldso_tls_static_space % sizeof(void*));
#endif

    tcb = ldso_allocate_tls_locked();

    __libc_set_tls_tcb(tcb);
}

struct tls_tcb* __ldso_allocate_tls(void) { return ldso_allocate_tls_locked(); }

LDSO_PUBLIC void* __tls_get_addr(void* arg_)
{
    size_t* arg = (size_t*)arg_;
    struct tls_tcb* tcb = __libc_get_tls_tcb();
    size_t idx = arg[0], offset = arg[1];
    void** dtv = tcb->tcb_dtv;

    if (idx < (size_t)dtv[-1] && dtv[idx] != NULL)
        return (uint8_t*)dtv[idx] + offset;

    return ldso_tls_get_addr(tcb, idx, offset);
}

#endif /* defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2) */
