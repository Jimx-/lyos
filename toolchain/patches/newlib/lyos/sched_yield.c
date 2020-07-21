#include <sched.h>
#include <errno.h>

int sched_yield(void) { return ENOSYS; }
