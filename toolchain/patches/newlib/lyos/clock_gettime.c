#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

int clock_gettime(clockid_t clock_id, struct timespec* tp) { return ENOSYS; }
