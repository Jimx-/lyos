#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <string.h>
#include <asm/ldt.h>

extern int syscall_entry(int syscall_nr, MESSAGE* m);

int set_thread_area(struct user_desc* desc)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));
    m.ADDR = desc;

    return syscall_entry(NR_SET_THREAD_AREA, &m);
}
