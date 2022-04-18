#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <stdio.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <lyos/iov_grant_iter.h>

void iov_grant_iter_init(struct iov_grant_iter* iter, endpoint_t granter,
                         const struct iovec_grant* iov, size_t nr_segs,
                         size_t count)
{
    iter->iov = iov;
    iter->count = count;
    iter->granter = granter;
    iter->nr_segs = nr_segs;
    iter->iov_offset = 0;
}

ssize_t iov_grant_iter_copy_from(struct iov_grant_iter* iter, void* buf,
                                 size_t bytes)
{
    size_t copied = 0;
    size_t chunk;
    int retval;

    while (copied < bytes && iter->nr_segs > 0) {
        chunk = min(iter->iov->iov_len - iter->iov_offset, bytes - copied);

        if (chunk > 0) {
            if ((retval = safecopy_from(iter->granter, iter->iov->iov_grant,
                                        iter->iov_offset, buf, chunk)) != OK)
                return -retval;
        }

        copied += chunk;
        iter->iov_offset += chunk;
        buf += chunk;

        if (iter->iov_offset == iter->iov->iov_len) {
            iter->iov++;
            iter->nr_segs--;
            iter->iov_offset = 0;
        }
    }

    return copied;
}

ssize_t iov_grant_iter_copy_to(struct iov_grant_iter* iter, const void* buf,
                               size_t bytes)
{
    size_t copied = 0;
    size_t chunk;
    int retval;

    while (copied < bytes && iter->nr_segs > 0) {
        chunk = min(iter->iov->iov_len - iter->iov_offset, bytes - copied);

        if (chunk > 0) {
            if ((retval = safecopy_to(iter->granter, iter->iov->iov_grant,
                                      iter->iov_offset, buf, chunk)) != OK)
                return -retval;
        }

        copied += chunk;
        iter->iov_offset += chunk;
        buf += chunk;

        if (iter->iov_offset == iter->iov->iov_len) {
            iter->iov++;
            iter->nr_segs--;
            iter->iov_offset = 0;
        }
    }

    return copied;
}
