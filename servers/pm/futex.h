#ifndef _PM_FUTEX_H_
#define _PM_FUTEX_H_

#include <lyos/types.h>
#include <lyos/list.h>

union futex_key {
    struct {
        unsigned long addr;
        struct pmproc* pmp;
        int offset;
    } private;
    struct {
        unsigned long word;
        void* ptr;
        int offset;
    } both;
};

struct futex_entry {
    struct list_head list;

    struct pmproc* pmp;
    union futex_key key;

    u32 bitset;
};

#endif
