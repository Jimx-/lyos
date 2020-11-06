#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/driver.h>
#include <lyos/proto.h>

#include <libdevman/libdevman.h>

int dm_class_register(const char* name, class_id_t* id)
{
    MESSAGE msg;

    msg.type = DM_CLASS_REGISTER;
    msg.BUF = (void*)name;
    msg.NAME_LEN = strlen(name);

    send_recv(BOTH, TASK_DEVMAN, &msg);

    if (msg.u.m_devman_register_reply.status == 0) {
        *id = msg.u.m_devman_register_reply.id;
    }

    return msg.u.m_devman_register_reply.status;
}
