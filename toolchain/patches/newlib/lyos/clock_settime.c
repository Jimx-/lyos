#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

int clock_settime(clockid_t clock_id, const struct timespec* tp)
{
    MESSAGE m;

    memset(&m, 0, sizeof(m));
    m.type = CLOCK_SETTIME;
    m.u.m_pm_time.clock_id = clock_id;
    m.u.m_pm_time.sec = tp->tv_sec;
    m.u.m_pm_time.nsec = tp->tv_nsec;

    send_recv(BOTH, TASK_PM, &m);

    if (m.RETVAL > 0) {
        errno = m.RETVAL;
        return -1;
    }

    return 0;
}
