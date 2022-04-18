#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

struct arch_watchdog {
    void (*init)(unsigned int);
    void (*reset)(unsigned int);
    int (*init_profile)(unsigned int);

    u64 reset_val;
    u64 watchdog_reset_val;
    u64 profile_reset_val;
};

extern int watchdog_enabled;
extern struct arch_watchdog* watchdog;

int arch_watchdog_init(void);
void arch_watchdog_stop(void);

#endif
