#ifndef _IDR_H_
#define _IDR_H_

#include <lyos/types.h>
#include <lyos/avl.h>

struct idr {
    struct avl_root root;
    unsigned int base;
    unsigned int next;
};

void idr_init_base(struct idr*, unsigned int base);
void idr_init(struct idr*);
int idr_alloc(struct idr*, void* ptr, int start, int end);
int idr_alloc_u32(struct idr* idr, void* ptr, u32* nextid, unsigned long max);
void* idr_remove(struct idr*, unsigned long id);
void* idr_find(const struct idr*, unsigned long id);
void idr_destroy(struct idr*);

#endif
