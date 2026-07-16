#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <string.h>

int vm_set_cacheblock(void* vaddr, dev_t dev, off_t dev_offset, ino_t ino,
                      off_t ino_offset, size_t len)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = MM_SET_CACHEBLOCK;
    msg.u.m_mm_cacheblock.who = SELF;
    msg.u.m_mm_cacheblock.vaddr = vaddr;
    msg.u.m_mm_cacheblock.dev = dev;
    msg.u.m_mm_cacheblock.dev_offset = dev_offset;
    msg.u.m_mm_cacheblock.ino = ino;
    msg.u.m_mm_cacheblock.ino_offset = ino_offset;
    msg.u.m_mm_cacheblock.len = len;

    send_recv(BOTH, TASK_MM, &msg);

    return msg.RETVAL;
}

void* vm_map_cacheblock(endpoint_t who, dev_t dev, off_t dev_offset, ino_t ino,
                        off_t ino_offset, size_t len)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = MM_MAP_CACHEBLOCK;
    msg.u.m_mm_cacheblock.who = who;
    msg.u.m_mm_cacheblock.dev = dev;
    msg.u.m_mm_cacheblock.dev_offset = dev_offset;
    msg.u.m_mm_cacheblock.ino = ino;
    msg.u.m_mm_cacheblock.ino_offset = ino_offset;
    msg.u.m_mm_cacheblock.len = len;

    send_recv(BOTH, TASK_MM, &msg);

    if (msg.RETVAL != 0) return MAP_FAILED;

    return msg.ADDR;
}
