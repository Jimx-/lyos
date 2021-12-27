#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <string.h>

extern int syscall_entry(int syscall_nr, MESSAGE* m);

int arch_prctl(int option, unsigned long arg2)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));
    m.u.m3.m3i1 = option;
    m.u.m3.m3l1 = arg2;

    return syscall_entry(NR_ARCH_PRCTL, &m);
}
