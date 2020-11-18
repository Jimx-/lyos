#include <sys/types.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/param.h>

extern struct sysinfo_user* __lyos_sysinfo;

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
    clock_t clock;
    unsigned long hz = __lyos_sysinfo->clock_info->hz;

    switch (clock_id) {
    case CLOCK_REALTIME:
        clock = __lyos_sysinfo->clock_info->realtime;
        break;
    case CLOCK_MONOTONIC:
        clock = __lyos_sysinfo->clock_info->uptime;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    tp->tv_sec = __lyos_sysinfo->clock_info->boottime + (clock / hz);
    tp->tv_nsec = (uint32_t)((uint64_t)(clock % hz) * (1000000000ULL / hz));

    return 0;
}
