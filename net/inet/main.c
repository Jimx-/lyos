#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>
#include <lyos/timer.h>

#include "inet.h"
#include "ifdev.h"

#define MAX_THREADS 128

struct idr sock_idr;

static int sys_hz;
static int check_timer;
static struct timer_list lwip_timer;

static sockid_t inet_socket(endpoint_t src, int domain, int type, int protocol,
                            struct sock** sock,
                            const struct sockdriver_ops** ops);
static void inet_poll(void);
static void inet_alarm(clock_t timestamp);
static void inet_other(MESSAGE* msg);

static void expire_lwip_timer(struct timer_list* timer);

static const struct sockdriver inet_driver = {
    .sd_create = inet_socket,
    .sd_poll = inet_poll,
    .sd_alarm = inet_alarm,
    .sd_other = inet_other,
};

uint32_t sys_now(void)
{
    clock_t now;

    check_timer = TRUE;

    get_ticks(&now, NULL);
    return (uint32_t)(((uint64_t)now * 1000) / sys_hz);
}

uint32_t lwip_hook_rand(void) { return lrand48(); }

static void set_lwip_timer(void)
{
    u32 next_timeout;
    clock_t ticks;

    next_timeout = sys_timeouts_sleeptime();

    if (next_timeout == (u32)-1) {
        cancel_timer(&lwip_timer);
    } else {
        if (next_timeout > INT_MAX / sys_hz)
            ticks = INT_MAX;
        else
            ticks = (next_timeout * sys_hz + 999) / 1000;

        cancel_timer(&lwip_timer);
        set_timer(&lwip_timer, ticks, expire_lwip_timer, NULL);
    }
}

static void expire_lwip_timer(struct timer_list* timer)
{
    sys_check_timeouts();
    set_lwip_timer();
    check_timer = FALSE;
}

static void check_lwip_timer(void)
{
    if (!check_timer) return;
    set_lwip_timer();
    check_timer = FALSE;
}

static sockid_t inet_socket(endpoint_t src, int domain, int type, int protocol,
                            struct sock** sock,
                            const struct sockdriver_ops** ops)
{
    switch (domain) {
    case PF_INET:
        switch (type) {
        case SOCK_STREAM:
            return tcpsock_socket(domain, protocol, sock, ops);
        case SOCK_DGRAM:
            return udpsock_socket(domain, protocol, sock, ops);
        default:
            return -EPROTOTYPE;
        }
        break;
    default:
        return -EAFNOSUPPORT;
    }
}

static void inet_poll(void)
{
    ifdev_poll();
    check_lwip_timer();
}

static void inet_alarm(clock_t timestamp) { expire_timer(timestamp); }

static void inet_other(MESSAGE* msg)
{
    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case TASK_SYSFS:
            ndev_check();
            break;
        }
        return;
    }

    if (IS_NDEV_CALL(msg->type)) {
        ndev_process(msg);
    }
}

static int inet_init(void)
{
    printl("inet: INET socket driver is running\n");

    srand48(time(NULL));

    idr_init(&sock_idr);

    sys_hz = get_system_hz();

    lwip_init();

    sockdriver_init();

    ndev_init();

    loopif_init();
    ifconf_init();

    init_timer(&lwip_timer);
    check_timer = TRUE;

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(inet_init);
    serv_init();

    sockdriver_task(&inet_driver, MAX_THREADS);

    return 0;
}
