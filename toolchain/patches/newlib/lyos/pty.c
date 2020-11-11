#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int posix_openpt(int oflag) { return open("/dev/ptmx", oflag); }

int grantpt(int fd) { return ioctl(fd, TIOCGRANTPT, 0); }

int unlockpt(int fd)
{
    int val = 0;
    return ioctl(fd, TIOCSPTLCK, &val);
}

int ptsname_r(int fd, char* buf, size_t buflen)
{
    unsigned int index;

    if (ioctl(fd, TIOCGPTN, &index) < 0) return -1;

    if (snprintf(buf, buflen, "/dev/pts/%u", index) >= buflen) {
        errno = ERANGE;
        return -1;
    }

    return 0;
}

char* ptsname(int fd)
{
    static char buf[128];

    if (ptsname_r(fd, buf, sizeof(buf)) < 0) return NULL;

    return buf;
}
