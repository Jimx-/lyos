#ifndef _KREF_H_
#define _KREF_H_

#include <asm/atomic.h>

struct kref {
    atomic_t refcount;
};

static inline void kref_init(struct kref* kref)
{
    INIT_ATOMIC(&kref->refcount, 1);
}

static inline void kref_get(struct kref* kref) { atomic_inc(&kref->refcount); }

static inline int kref_put(struct kref* kref,
                           void (*release)(struct kref* kref))
{
    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }

    return 0;
}

#endif
