#include <errno.h>
#include <ftw.h>

int ftw(const char* path, int (*fn)(const char*, const struct stat*, int),
        int fd_limit)
{
    return ENOSYS;
}

int nftw(const char* path,
         int (*fn)(const char*, const struct stat*, int, struct FTW*),
         int fd_limit, int flags)
{
    return ENOSYS;
}
