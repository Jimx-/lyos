#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <lyos/fs.h>
#include <errno.h>
#include <string.h>

int copyfd(endpoint_t who, int fd, int how)
{
    MESSAGE m;

    memset(&m, 0, sizeof(m));
    m.type = VFS_COPYFD;
    m.u.m_vfs_copyfd.endpoint = who;
    m.u.m_vfs_copyfd.fd = fd;
    m.u.m_vfs_copyfd.how = how;

    send_recv(BOTH, TASK_FS, &m);

    return m.RETVAL;
}
