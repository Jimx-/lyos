#include <stdio.h>
#include <sys/uio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_read(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.FD);
    } else {
        if (tcp->msg_out.RETVAL >= 0)
            print_strn(tcp, tcp->msg_in.BUF, tcp->msg_out.RETVAL);
        else
            print_addr((uint64_t)tcp->msg_in.BUF);

        printf(", %d", tcp->msg_in.CNT);
    }

    return 0;
}

int trace_write(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, ", msg->FD);
    print_strn(tcp, msg->BUF, msg->CNT);
    printf(", %d", msg->CNT);

    return RVAL_DECODED;
}

struct print_iovec_config {
    unsigned long data_size;
};

static int print_iovec(struct tcb* tcp, void* elem_buf, size_t elem_size,
                       void* data)
{
    const struct iovec* iov = elem_buf;
    unsigned long len;
    struct print_iovec_config* config = data;

    printf("{iov_base=");

    len = iov->iov_len;

    if (len > config->data_size) len = config->data_size;
    if (config->data_size != (unsigned long)-1) config->data_size -= len;
    print_strn(tcp, iov->iov_base, len);

    printf(", iov_len=%lu}", iov->iov_len);

    return 1;
}

void print_iov_upto(struct tcb* const tcp, size_t len, void* addr,
                    unsigned long data_size)
{
    struct iovec iov;
    struct print_iovec_config config = {.data_size = data_size};

    print_array(tcp, addr, len, &iov, sizeof(iov), fetch_mem, print_iovec,
                &config);
}
