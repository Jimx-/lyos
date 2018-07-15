#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syslimits.h>

long sysconf(int name)
{
	switch (name) {
	case _SC_ARG_MAX:
		return ARG_MAX;
    case _SC_PAGE_SIZE:
        return getpagesize();
	default:
		printf("sysconf %d not implemented\n", name);
		break;
	}
	return -ENOSYS;
}
