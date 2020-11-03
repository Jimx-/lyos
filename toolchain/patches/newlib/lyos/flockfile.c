#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int flock(int fd, int operation)
{
    struct flock lock;

    memset(&lock, 0, sizeof(lock));
    switch (operation & ~LOCK_NB) {
    case LOCK_SH:
        lock.l_type = F_RDLCK;
        break;
    case LOCK_EX:
        lock.l_type = F_WRLCK;
        break;
    case LOCK_UN:
        lock.l_type = F_UNLCK;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    return fcntl(fd, operation & LOCK_NB ? F_SETLK : F_SETLKW, &lock);
}

void flockfile(FILE* filehandle) { flock(fileno(filehandle), LOCK_EX); }

int ftrylockfile(FILE* filehandle)
{
    return flock(fileno(filehandle), LOCK_EX | LOCK_NB) == 0;
}

void funlockfile(FILE* filehandle) { flock(fileno(filehandle), LOCK_UN); }

int lockf(int fd, int cmd, off_t len)
{
    struct flock fl = {.l_type = F_WRLCK, .l_whence = SEEK_CUR, .l_len = len};

    switch (cmd) {
    case F_TEST:
        fl.l_type = F_RDLCK;
        if (fcntl(fd, F_GETLK, &fl) < 0) return -1;
        if (fl.l_type == F_UNLCK || fl.l_pid == getpid()) return 0;

        errno = EACCES;
        return -1;
    case F_ULOCK:
        fl.l_type = F_UNLCK;
        return fcntl(fd, F_SETLK, &fl);
    case F_LOCK:
        return fcntl(fd, F_SETLKW, &fl);
    case F_TLOCK:
        return fcntl(fd, F_SETLK, &fl);
    }

    errno = EINVAL;
    return -1;
}
