#ifndef _SCONST_64_H_
#define _SCONST_64_H_

/* clang-format off */

#define CLEAR_IF(where) \
    mov where, %eax ;\
    andl $0xfffffdff, %eax ;\
    mov %eax, where ;

/* clang-format on */

#endif
