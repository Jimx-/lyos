#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <string.h>

int mapdriver(const char* label, const int* domains, int nr_domains)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = VFS_MAPDRIVER;
    msg.u.m_vfs_mapdriver.label = label;
    msg.u.m_vfs_mapdriver.label_len = strlen(label);
    msg.u.m_vfs_mapdriver.domains = domains;
    msg.u.m_vfs_mapdriver.nr_domains = nr_domains;

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}
