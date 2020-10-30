#ifndef _UIO_H_
#define _UIO_H_

#include <sys/types.h>

struct iovec {
    void* iov_base;
    size_t iov_len;
};

ssize_t writev(int, const struct iovec*, int);

#endif
