#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#define MSEC_PER_SEC  1000
#define USEC_PER_MSEC 1000
#define NSEC_PER_USEC 1000

#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#define NSEC_PER_SEC (NSEC_PER_USEC * USEC_PER_SEC)

int usleep(useconds_t usec)
{
    struct timespec ts;
    ts.tv_sec = usec / USEC_PER_SEC;
    ts.tv_nsec = (usec % USEC_PER_SEC) * NSEC_PER_USEC;
    return nanosleep(&ts, NULL);
}
