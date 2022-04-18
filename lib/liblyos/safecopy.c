#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>

int safecopy_from(endpoint_t src_ep, mgrant_id_t grant, off_t offset,
                  void* addr, size_t len)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.u.m_safecopy.src_dest = src_ep;
    msg.u.m_safecopy.grant = grant;
    msg.u.m_safecopy.offset = offset;
    msg.u.m_safecopy.addr = addr;
    msg.u.m_safecopy.len = len;

    return syscall_entry(NR_SAFECOPYFROM, &msg);
}

int safecopy_to(endpoint_t dest_ep, mgrant_id_t grant, off_t offset,
                const void* addr, size_t len)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.u.m_safecopy.src_dest = dest_ep;
    msg.u.m_safecopy.grant = grant;
    msg.u.m_safecopy.offset = offset;
    msg.u.m_safecopy.addr = (void*)addr;
    msg.u.m_safecopy.len = len;

    return syscall_entry(NR_SAFECOPYTO, &msg);
}
