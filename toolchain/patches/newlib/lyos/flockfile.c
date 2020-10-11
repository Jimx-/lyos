#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

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
