#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>

int name_to_handle_at(int dirfd, const char* pathname,
                      struct file_handle* handle, int* mount_id, int flags)
{
    errno = ENOSYS;
    return -1;
}

int open_by_handle_at(int mount_fd, struct file_handle* handle, int flags)
{
    errno = ENOSYS;
    return -1;
}
