#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#define MSEC_PER_SEC  1000
#define USEC_PER_MSEC 1000
#define NSEC_PER_USEC 1000

#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#define NSEC_PER_SEC (NSEC_PER_USEC * USEC_PER_SEC)

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    struct timeval timeout, start, end;
    int retval;

    if (!req) {
        errno = EFAULT;
        return -1;
    }

    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= NSEC_PER_SEC) {
        errno = EINVAL;
        return -1;
    }

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
        if (gettimeofday(&start, NULL) < 0) return -1;
    }

    timeout.tv_sec = req->tv_sec;
    timeout.tv_usec = (req->tv_nsec + NSEC_PER_USEC - 1) / NSEC_PER_USEC;
    retval = select(0, NULL, NULL, NULL, &timeout);

    if (!rem || retval >= 0) {
        return 0;
    }

    if (gettimeofday(&end, NULL) < 0) return -1;

    rem->tv_sec = req->tv_sec - (end.tv_sec - start.tv_sec);
    rem->tv_nsec = req->tv_nsec - (end.tv_usec - start.tv_usec) * NSEC_PER_USEC;

    while (rem->tv_nsec < 0) {
        rem->tv_sec -= 1;
        rem->tv_nsec += NSEC_PER_SEC;
    }

    while (rem->tv_nsec > NSEC_PER_SEC) {
        rem->tv_sec += 1;
        rem->tv_nsec -= NSEC_PER_SEC;
    }

    if (rem->tv_sec < 0) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return retval;
}
