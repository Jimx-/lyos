#include <stdio.h>
#include <sys/mman.h>

#include "types.h"
#include "proto.h"

#include "xlat/mmap_prot.h"
#include "xlat/mmap_flags.h"
#include "xlat/mremap_flags.h"

int trace_brk(struct tcb* tcp)
{
    print_addr((uint64_t)tcp->msg_in.ADDR);

    return RVAL_DECODED | RVAL_HEX;
}

int trace_mmap(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_addr((uint64_t)msg->u.m_mm_mmap.vaddr);
    printf(", ");
    printf("%ld", msg->u.m_mm_mmap.length);
    printf(", ");
    print_flags(msg->u.m_mm_mmap.prot, &mmap_prot);
    printf(", ");
    print_flags(msg->u.m_mm_mmap.flags, &mmap_flags);
    printf(", ");
    printf("%d, %ld", msg->u.m_mm_mmap.fd, msg->u.m_mm_mmap.offset);

    return RVAL_DECODED | RVAL_SPECIAL;
}

int trace_munmap(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_addr((uint64_t)msg->u.m_mm_mmap.vaddr);
    printf(", ");
    printf("%ld", msg->u.m_mm_mmap.length);

    return RVAL_DECODED;
}

int trace_mremap(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_addr((uint64_t)msg->u.m_mm_mremap.old_addr);
    printf(", ");
    printf("%ld", msg->u.m_mm_mremap.old_size);
    printf(", ");
    printf("%ld", msg->u.m_mm_mremap.new_size);
    printf(", ");
    print_flags(msg->u.m_mm_mremap.flags, &mremap_flags);

    if (msg->u.m_mm_mremap.flags & MREMAP_FIXED) {
        printf(", ");
        print_addr((uint64_t)msg->u.m_mm_mremap.new_addr);
    }

    return RVAL_DECODED | RVAL_SPECIAL;
}
