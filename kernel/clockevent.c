#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/const.h"
#include <errno.h>
#include <kernel/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <kernel/clockevent.h>
#include <lyos/time.h>
#include <asm/div64.h>
#include <lyos/spinlock.h>

static DEF_SPINLOCK(clockevents_lock);
static DEF_LIST(clockevent_devices);

static u64 cev_delta2ns(struct clock_event_device* evt, unsigned long delta,
                        int ismax)
{
    u64 clc = (u64)delta << evt->shift;
    u64 rnd;

    rnd = (u64)evt->mult - 1;

    if ((clc >> evt->shift) != (u64)delta) clc = ~0ULL;

    if ((~0ULL - clc > rnd) && (!ismax || evt->mult <= (1ULL << evt->shift)))
        clc += rnd;

    do_div(clc, evt->mult);

    return clc > 1000 ? clc : 1000;
}

u64 clockevent_delta2ns(struct clock_event_device* evt, unsigned long delta)
{
    return cev_delta2ns(evt, delta, FALSE);
}

void clockevents_handle_noop(struct clock_event_device* dev) {}

void clockevents_register_device(struct clock_event_device* dev)
{
    dev->state = CLOCK_EVT_STATE_DETACHED;

    spinlock_lock(&clockevents_lock);
    list_add(&dev->list, &clockevent_devices);
    tick_check_new_device(dev);
    spinlock_unlock(&clockevents_lock);
}

static void calc_clock_mul_shift(u32* mul, u32* shift, u32 from, u32 to,
                                 u32 maxsec)
{
    u64 tmp;
    u32 sft, sftacc = 32;

    tmp = ((u64)maxsec * from) >> 32;
    while (tmp) {
        tmp >>= 1;
        sftacc--;
    }

    for (sft = 32; sft > 0; sft--) {
        tmp = (u64)to << sft;
        tmp += from / 2;
        do_div(tmp, from);
        if ((tmp >> sftacc) == 0) break;
    }
    *mul = tmp;
    *shift = sft;
}

static void clockevents_config(struct clock_event_device* dev, u32 freq)
{
    u64 sec;

    sec = dev->max_delta_ticks;
    do_div(sec, freq);
    if (!sec)
        sec = 1;
    else if (sec > 600 && dev->max_delta_ticks > 0xffffffff)
        sec = 600;

    calc_clock_mul_shift(&dev->mult, &dev->shift, NSEC_PER_SEC, freq, sec);
    dev->min_delta_ns = cev_delta2ns(dev, dev->min_delta_ticks, FALSE);
    dev->max_delta_ns = cev_delta2ns(dev, dev->max_delta_ticks, TRUE);
}

void clockevents_config_and_register(struct clock_event_device* dev, u32 freq,
                                     unsigned long min_delta,
                                     unsigned long max_delta)
{
    dev->min_delta_ticks = min_delta;
    dev->max_delta_ticks = max_delta;
    clockevents_config(dev, freq);
    clockevents_register_device(dev);
}

static int __clockevents_switch_state(struct clock_event_device* dev,
                                      enum clock_event_state state)
{
    switch (state) {
    case CLOCK_EVT_STATE_DETACHED:
    case CLOCK_EVT_STATE_SHUTDOWN:
        if (dev->set_state_shutdown) return dev->set_state_shutdown(dev);
        return 0;

    case CLOCK_EVT_STATE_PERIODIC:
        if (dev->set_state_periodic) return dev->set_state_periodic(dev);
        return 0;

    case CLOCK_EVT_STATE_ONESHOT:
        if (dev->set_state_oneshot) return dev->set_state_oneshot(dev);
        return 0;

    case CLOCK_EVT_STATE_ONESHOT_STOPPED:
        if (dev->set_state_oneshot_stopped)
            return dev->set_state_oneshot_stopped(dev);
        return 0;

    default:
        return -ENOSYS;
    }
}

void clockevents_switch_state(struct clock_event_device* dev,
                              enum clock_event_state state)
{
    if (dev->state != state) {
        if (__clockevents_switch_state(dev, state)) return;

        dev->state = state;
    }
}

void clockevents_shutdown(struct clock_event_device* dev)
{
    clockevents_switch_state(dev, CLOCK_EVT_STATE_SHUTDOWN);
}

void clockevents_exchange_device(struct clock_event_device* old,
                                 struct clock_event_device* new)
{
    if (old) clockevents_switch_state(old, CLOCK_EVT_STATE_DETACHED);

    if (new) clockevents_shutdown(new);
}

int clockevents_program_delta(struct clock_event_device* dev, u64 delta_ns)
{
    unsigned long long clc;
    int64_t delta = delta_ns;

    if (dev->state == CLOCK_EVT_STATE_SHUTDOWN) return 0;

    delta = min(delta, (int64_t)dev->max_delta_ns);
    delta = max(delta, (int64_t)dev->min_delta_ns);

    clc = ((unsigned long long)delta * dev->mult) >> dev->shift;
    return dev->set_next_event(dev, (unsigned long)clc);
}
