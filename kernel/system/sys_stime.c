#include <lyos/const.h>
#include <lyos/ipc.h>
#include <kernel/proto.h>

int sys_stime(MESSAGE* m, struct proc* p_proc)
{
    set_boottime(m->u.m_stime.boot_time);

    return 0;
}
