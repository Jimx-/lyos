#include <errno.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>

#include "inet.h"

int is_superuser(endpoint_t endpoint)
{
    uid_t euid;
    get_epinfo(endpoint, &euid, NULL);

    return euid == SU_UID;
}

int convert_err(err_t err)
{
    switch (err) {
    case ERR_OK:
        return 0;
    case ERR_MEM:
        return ENOMEM;
    case ERR_BUF:
        return ENOBUFS;
    case ERR_TIMEOUT:
        return ETIMEDOUT;
    case ERR_RTE:
        return EHOSTUNREACH;
    case ERR_VAL:
        return EINVAL;
    case ERR_USE:
        return EADDRINUSE;
    case ERR_ALREADY:
        return EALREADY;
    case ERR_ISCONN:
        return EISCONN;
    case ERR_CONN:
        return ENOTCONN;
    case ERR_IF:
        return ENETDOWN;
    case ERR_ABRT:
        return ECONNABORTED;
    case ERR_RST:
        return ECONNRESET;
    case ERR_INPROGRESS:
        return EINPROGRESS;
    case ERR_WOULDBLOCK:
        return EWOULDBLOCK;
    case ERR_ARG:
        return EINVAL;
    case ERR_CLSD:
    default:
        return EINVAL;
    }
}

ssize_t copy_socket_data(struct iov_grant_iter* iter, size_t len,
                         const struct pbuf* pbuf, size_t skip, int copy_in)
{
    size_t off = 0;
    ssize_t retval;

    while (len > 0) {
        size_t chunk;

        chunk = pbuf->len - skip;
        if (chunk > len) chunk = len;

        if (copy_in)
            retval = iov_grant_iter_copy_from(iter, (char*)pbuf->payload + skip,
                                              chunk);
        else
            retval = iov_grant_iter_copy_to(iter, (char*)pbuf->payload + skip,
                                            chunk);

        if (retval < 0) return retval;

        pbuf = pbuf->next;
        len -= chunk;
        off += chunk;
        skip = 0;
    }

    return off;
}
