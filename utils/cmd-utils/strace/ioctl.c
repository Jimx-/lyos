#include <stdio.h>
#include <lyos/ioctl.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/ioctl_dirs.h"

static void ioctl_decode(int code)
{
    printf("_IOC(");
    print_flags(_IOC_DIR(code), &ioctl_dirs);
    printf(", 0x%x, 0x%x, 0x%x)", _IOC_TYPE(code), _IOC_NR(code),
           _IOC_SIZE(code));
}

int trace_ioctl(struct tcb* tcp)
{
    int fd = tcp->msg_in.FD;
    int request = tcp->msg_in.u.m3.m3l1;
    void* buf = tcp->msg_in.BUF;

    printf("%d, ", fd);

    ioctl_decode(request);
    printf(", ");

    print_addr((uint64_t)buf);

    return RVAL_DECODED;
}
