#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>
#include <errno.h>
#include <string.h>

int socketpath(endpoint_t endpoint, const char* path, size_t size, int request,
               dev_t* dev, ino_t* ino)
{
    MESSAGE msg;
    mgrant_id_t grant;
    int retval;

    grant = mgrant_set_direct(TASK_FS, (vir_bytes)path, size, MGF_READ);
    if (grant == GRANT_INVALID) return ENOMEM;

    memset(&msg, 0, sizeof(msg));
    msg.type = VFS_SOCKETPATH;
    msg.u.m_vfs_socketpath.endpoint = endpoint;
    msg.u.m_vfs_socketpath.grant = grant;
    msg.u.m_vfs_socketpath.size = size;
    msg.u.m_vfs_socketpath.request = request;

    send_recv(BOTH, TASK_FS, &msg);

    retval = msg.u.m_vfs_socketpath.status;
    mgrant_revoke(grant);

    if (retval == OK) {
        *dev = msg.u.m_vfs_socketpath.dev;
        *ino = msg.u.m_vfs_socketpath.num;
    }

    return retval;
}
