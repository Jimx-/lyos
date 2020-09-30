#include <stdio.h>
#include <sys/mman.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/mmap_prot.h"
#include "xlat/mmap_flags.h"

int trace_brk(struct tcb* tcp)
{
    printf("brk(");
    print_addr((uint64_t)tcp->msg_in.ADDR);
    printf(")");

    return RVAL_DECODED;
}

int trace_mmap(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("mmap(");
    print_addr((uint64_t)msg->u.m_mm_mmap.vaddr);
    printf(", ");
    printf("%ld", msg->u.m_mm_mmap.length);
    printf(", ");
    print_flags(msg->u.m_mm_mmap.prot, &mmap_prot);
    printf(", ");
    print_flags(msg->u.m_mm_mmap.flags, &mmap_flags);
    printf(", ");
    printf("%d, %d, %ld)", msg->u.m_mm_mmap.fd, msg->u.m_mm_mmap.offset);

    return RVAL_DECODED | RVAL_HEX;
}

int trace_munmap(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("munmap(");
    print_addr((uint64_t)msg->u.m_mm_mmap.vaddr);
    printf(", ");
    printf("%ld", msg->u.m_mm_mmap.length);
    printf(")");

    return RVAL_DECODED;
}
