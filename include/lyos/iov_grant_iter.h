#ifndef _LYOS_IOV_GRANT_ITER_H_
#define _LYOS_IOV_GRANT_ITER_H_

#include <sys/types.h>
#include <lyos/types.h>

struct iov_grant_iter {
    size_t iov_offset;
    size_t count;
    endpoint_t granter;
    const struct iovec_grant* iov;
    size_t nr_segs;
};

void iov_grant_iter_init(struct iov_grant_iter* iter, endpoint_t granter,
                         const struct iovec_grant* iov, size_t nr_segs,
                         size_t count);

ssize_t iov_grant_iter_copy_from(struct iov_grant_iter* iter, void* buf,
                                 size_t bytes);
ssize_t iov_grant_iter_copy_to(struct iov_grant_iter* iter, const void* buf,
                               size_t bytes);

#endif
