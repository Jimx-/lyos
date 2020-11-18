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

int gettimeofday(struct timeval* tv, void* tz)
{
    clock_t clock;
    unsigned long hz = __lyos_sysinfo->clock_info->hz;

    clock = __lyos_sysinfo->clock_info->realtime;

    tv->tv_sec = __lyos_sysinfo->clock_info->boottime + (clock / hz);
    tv->tv_usec = (uint32_t)((uint64_t)(clock % hz) * (1000000ULL / hz));

    return 0;
}
