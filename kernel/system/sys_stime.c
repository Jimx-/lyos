#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <lyos/proc.h>
#include <lyos/timer.h>
#include <lyos/proto.h>

int sys_stime(MESSAGE* m, struct proc* p_proc)
{
    set_boottime(m->u.m_stime.boot_time);

    return 0;
}
