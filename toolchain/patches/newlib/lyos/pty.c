#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <unistd.h>

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

int openpty(int* amaster, int* aslave, char* name, const struct termios* termp,
            const struct winsize* winp)
{
    char buf[128];
    int fd, pts_fd;

    fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

    if (fd < 0) return -1;

    if (ptsname_r(fd, buf, sizeof(buf)) < 0) return -1;

    pts_fd = open(buf, O_RDWR | O_NOCTTY);

    if (pts_fd < 0) return -1;

    *amaster = fd;
    *aslave = pts_fd;
    return 0;
}

int login_tty(int fd)
{
    if (setsid() < 0) return -1;

    if (ioctl(fd, TIOCSCTTY, 0)) return -1;

    if (dup2(fd, STDIN_FILENO) < 0) return -1;
    if (dup2(fd, STDOUT_FILENO) < 0) return -1;
    if (dup2(fd, STDERR_FILENO) < 0) return -1;

    if (close(fd) < 0) return -1;

    return 0;
}

pid_t forkpty(int* amaster, char* name, const struct termios* termp,
              const struct winsize* winp)
{
    int pts_fd;
    pid_t child;

    if (openpty(amaster, &pts_fd, name, termp, winp) < 0) return -1;

    child = fork();
    if (child < 0) return -1;

    if (!child) {
        login_tty(pts_fd);
    } else {
        if (close(pts_fd) < 0) return -1;
    }

    return child;
}
