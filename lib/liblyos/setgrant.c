#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>

int setgrant(mgrant_t* grants, size_t size)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.BUF = grants;
    msg.CNT = size;

    return syscall_entry(NR_SETGRANT, &msg);
}
