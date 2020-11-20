#ifndef _SYS_UIO_H_
#define _SYS_UIO_H_

#include <sys/types.h>
#include <sys/cdefs.h>

struct iovec {
    void* iov_base;
    size_t iov_len;
};

__BEGIN_DECLS

ssize_t readv(int, const struct iovec*, int);
ssize_t writev(int, const struct iovec*, int);

__END_DECLS

#endif
