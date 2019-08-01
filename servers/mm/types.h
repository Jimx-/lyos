#ifndef _MM_TYPES_H_
#define _MM_TYPES_H_

#include <lyos/type.h>

struct hole {
    struct hole * h_next;
    vir_bytes h_base;
    size_t h_len;
};

struct phys_hole {
    struct phys_hole * h_next;
    phys_bytes h_base;
    phys_bytes h_len;
};

#endif // _MM_TYPES_H_
