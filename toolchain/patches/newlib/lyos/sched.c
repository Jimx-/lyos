#include <sched.h>
#include <errno.h>

int sched_yield(void) { return 0; }

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t* mask)
{
    errno = ENOSYS;
    return -1;
}

int __cpu_isset(int cpu, cpu_set_t* set) { return 0; }

int __cpu_count(cpu_set_t* set) { return 1; }
