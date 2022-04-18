#include <lyos/ipc.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <time.h>

int kernel_stime(time_t boot_time)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.u.m_stime.boot_time = boot_time;

    return syscall_entry(NR_STIME, &msg);
}
