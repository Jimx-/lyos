#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define DIRBLKSIZ 1024
DIR* opendir(const char* name)
{
    int fd;
    DIR* dirp;

    fd = open(name, O_RDONLY);
    if (fd == -1) return NULL;

    dirp = fdopendir(fd);
    if (!dirp) close(fd);

    return dirp;
}

DIR* fdopendir(int fd)
{
    struct stat sbuf;

    if (fstat(fd, &sbuf)) {
        errno = ENOTDIR;
        return NULL;
    }
    if (!S_ISDIR(sbuf.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }

    memset(dir, 0, sizeof(*dir));
    dir->len = DIRBLKSIZ;
    dir->buf = malloc(dir->len);
    if (!dir->buf) {
        errno = ENOMEM;
        free(dir);
        return NULL;
    }

    dir->loc = 0;
    dir->fd = fd;

    return dir;
}

struct dirent* readdir(DIR* dirp)
{
    struct dirent* dp;

    if (!dirp) return NULL;

    if (dirp->loc >= dirp->size) dirp->loc = 0;
    if (dirp->loc == 0) {
        if ((dirp->size = getdents(dirp->fd, dirp->buf, dirp->len)) <= 0)
            return NULL;
    }

    dp = (struct dirent*)((char*)dirp->buf + dirp->loc);
    if (dp->d_reclen >= dirp->len - dirp->loc + 1) return NULL;
    dirp->loc += dp->d_reclen;

    return dp;
}

int readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result)
{
    struct dirent* dp;

    if (!dirp) return NULL;

    if (dirp->loc >= dirp->size) dirp->loc = 0;
    if (dirp->loc == 0) {
        dirp->size = getdents(dirp->fd, dirp->buf, dirp->len);

        if (dirp->size < 0) {
            return -dirp->size;
        }

        if (dirp->size == 0) {
            *result = NULL;
            return 0;
        }
    }

    dp = (struct dirent*)((char*)dirp->buf + dirp->loc);

    if (dp->d_reclen >= dirp->len - dirp->loc + 1) return ENAMETOOLONG;

    memcpy(entry, dp, offsetof(struct dirent, d_name) + strlen(dp->d_name) + 1);
    dirp->loc += dp->d_reclen;

    *result = entry;
    return 0;
}

void rewinddir(DIR* dirp) { dirp->loc = dirp->size = 0; }

int closedir(DIR* dirp)
{
    int fd = dirp->fd;

    free(dirp->buf);
    free(dirp);

    return close(fd);
}
