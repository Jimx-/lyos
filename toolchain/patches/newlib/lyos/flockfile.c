#include <stdio.h>
#include <errno.h>

int flock(int fd, int operation)
{
    errno = ENOSYS;
    return -1;
}

void flockfile(FILE* filehandle) {}

int ftrylockfile(FILE* filehandle) { return 1; }

void funlockfile(FILE* filehandle) {}
