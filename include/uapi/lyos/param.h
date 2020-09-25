#ifndef _UAPI_LYOS_PARAM_H_
#define _UAPI_LYOS_PARAM_H_

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE* m);

struct kclockinfo {
    time_t boottime;
    clock_t uptime;
    clock_t realtime;
    unsigned int hz;
};

struct sysinfo_user {
#define SYSINFO_MAGIC 0x534946
    int magic;

    syscall_gate_t syscall_gate;
    struct kclockinfo* clock_info;
};

#endif
