#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

int clock_settime(clockid_t clock_id, const struct timespec* tp)
{
    return ENOSYS;
}
