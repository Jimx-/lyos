#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syslimits.h>
#include <sys/resource.h>

long sysconf(int name)
{
    struct rlimit rl;

    switch (name) {
    case _SC_ARG_MAX:
        return ARG_MAX;
    case _SC_PAGE_SIZE:
        return getpagesize();
    case _SC_OPEN_MAX:
        return (getrlimit(RLIMIT_NOFILE, &rl) == 0 ? rl.rlim_cur : -1);
    default:
        printf("sysconf %d not implemented\n", name);
        break;
    }

    errno = EINVAL;
    return -1;
}
