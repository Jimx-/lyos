#include <sys/prctl.h>
#include <errno.h>

int prctl(int option, ...)
{
    errno = ENOSYS;
    return -1;
}
