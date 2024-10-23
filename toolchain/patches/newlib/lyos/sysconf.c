#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syslimits.h>
#include <sys/resource.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/param.h>

extern struct sysinfo_user* __lyos_sysinfo;

long sysconf(int name)
{
    struct rlimit rl;

    switch (name) {
    case _SC_CHILD_MAX:
        return (getrlimit(RLIMIT_NPROC, &rl) == 0 ? rl.rlim_cur : -1);
    case _SC_ARG_MAX:
        return ARG_MAX;
    case _SC_PAGE_SIZE:
        return getpagesize();
    case _SC_OPEN_MAX:
        return (getrlimit(RLIMIT_NOFILE, &rl) == 0 ? rl.rlim_cur : -1);
    case _SC_CLK_TCK:
        return __lyos_sysinfo->clock_info->hz;
    case _SC_NGROUPS_MAX:
        return NGROUPS_MAX;
    case _SC_LINE_MAX:
        return LINE_MAX;
    case _SC_MONOTONIC_CLOCK:
        return 1;
    default:
        printf("sysconf %d not implemented\n", name);
        break;
    }

    errno = EINVAL;
    return -1;
}
