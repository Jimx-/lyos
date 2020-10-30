#include <sys/uio.h>
#include <unistd.h>

ssize_t writev(int fildes, const struct iovec* iov, int iovcnt)
{
    int i, retval;
    ssize_t bytes;

    bytes = 0;
    for (i = 0; i < iovcnt; i++) {
        retval = write(fildes, iov[i].iov_base, iov[i].iov_len);
        if (retval < 0) return retval;

        bytes += retval;
    }

    return bytes;
}
