#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

int clock_getres(clockid_t clock_id, struct timespec* res) { return ENOSYS; }
