#ifndef _CLOCKEVENT_H_
#define _CLOCKEVENT_H_

#include <lyos/list.h>
#include <kernel/cpumask.h>

enum clock_event_state {
    CLOCK_EVT_STATE_DETACHED,
    CLOCK_EVT_STATE_SHUTDOWN,
    CLOCK_EVT_STATE_PERIODIC,
    CLOCK_EVT_STATE_ONESHOT,
    CLOCK_EVT_STATE_ONESHOT_STOPPED,
};

struct clock_event_device {
    struct list_head list;

    u64 max_delta_ns;
    u64 min_delta_ns;
    u32 mult;
    u32 shift;
    enum clock_event_state state;

    unsigned long min_delta_ticks;
    unsigned long max_delta_ticks;

    const cpumask_t* cpumask;
    int rating;

    void (*event_handler)(struct clock_event_device*);
    int (*set_next_event)(struct clock_event_device*, unsigned long delta);

    int (*set_state_periodic)(struct clock_event_device*);
    int (*set_state_oneshot)(struct clock_event_device*);
    int (*set_state_oneshot_stopped)(struct clock_event_device*);
    int (*set_state_shutdown)(struct clock_event_device*);
};

void clockevents_register_device(struct clock_event_device* dev);
void clockevents_config_and_register(struct clock_event_device* dev, u32 freq,
                                     unsigned long min_delta,
                                     unsigned long max_delta);

int clockevents_program_delta(struct clock_event_device* dev, u64 delta_ns);

void clockevents_switch_state(struct clock_event_device* dev,
                              enum clock_event_state state);
void clockevents_shutdown(struct clock_event_device* dev);

void clockevents_exchange_device(struct clock_event_device* old,
                                 struct clock_event_device* new);

void clockevents_handle_noop(struct clock_event_device* dev);

void tick_check_new_device(struct clock_event_device* dev);

#endif
