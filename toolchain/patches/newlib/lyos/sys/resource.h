#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#include <sys/time.h>
#include <stdint.h>

#define RUSAGE_SELF     0  /* calling process */
#define RUSAGE_CHILDREN -1 /* terminated child processes */

struct rusage {
    struct timeval ru_utime; /* user time used */
    struct timeval ru_stime; /* system time used */
};

/*
 * Resource limits
 */
#define RLIMIT_CPU        0  /* cpu time in milliseconds */
#define RLIMIT_FSIZE      1  /* maximum file size */
#define RLIMIT_DATA       2  /* data size */
#define RLIMIT_STACK      3  /* stack size */
#define RLIMIT_CORE       4  /* core file size */
#define RLIMIT_RSS        5  /* resident set size */
#define RLIMIT_MEMLOCK    6  /* locked-in-memory address space */
#define RLIMIT_NPROC      7  /* number of processes */
#define RLIMIT_NOFILE     8  /* number of open files */
#define RLIMIT_SBSIZE     9  /* maximum size of all socket buffers */
#define RLIMIT_AS         10 /* virtual process size (inclusive of mmap) */
#define RLIMIT_VMEM       RLIMIT_AS /* common alias */
#define RLIMIT_NTHR       11        /* number of threads */
#define RLIMIT_LOCKS      12
#define RLIMIT_SIGPENDING 13
#define RLIMIT_MSGQUEUE   14
#define RLIMIT_NICE       15
#define RLIMIT_RTPRIO     16
#define RLIMIT_NLIMITS    17

#define RLIM_INFINITY (~((uint64_t)1 << 63)) /* no limit */

typedef uint64_t rlim_t;

struct rlimit {
    rlim_t rlim_cur; /* Soft limit */
    rlim_t rlim_max; /* Hard limit (ceiling for rlim_cur) */
};

#ifdef __cplusplus
extern "C"
{
#endif

    int getrusage(int, struct rusage*);
    int getrlimit(int resource, struct rlimit* rlim);
    int setrlimit(int resource, const struct rlimit* rlim);

#ifdef __cplusplus
}
#endif

#endif
