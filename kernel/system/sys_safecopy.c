#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <lyos/const.h>
#include <lyos/priv.h>
#include <lyos/mgrant.h>
#include <errno.h>

#define MAX_INDIRECT_DEPTH 5

int verify_grant(endpoint_t granter, endpoint_t grantee, mgrant_id_t grant,
                 size_t bytes, int access, vir_bytes offset, vir_bytes* voffset,
                 endpoint_t* new_granter)
{
    struct proc* granter_proc;
    struct priv* granter_priv;
    mgrant_t grant_entry;
    int grant_idx, grant_seq;
    int depth = 0;

    do {
        if (grant == GRANT_INVALID) return EINVAL;
        if (!(granter_proc = endpt_proc(granter))) return EINVAL;

        granter_priv = granter_proc->priv;
        if (!granter_priv) return EPERM;

        if (granter_priv->grant_endpoint != granter_proc->endpoint)
            return EAGAIN;

        if (!granter_priv->grant_table) return EPERM;

        grant_idx = GRANT_IDX(grant);
        grant_seq = GRANT_SEQ(grant);

        if (grant_idx >= granter_priv->grant_entries) return EPERM;

        if (data_vir_copy(KERNEL, &grant_entry, granter,
                          (void*)(granter_priv->grant_table +
                                  grant_idx * sizeof(grant_entry)),
                          sizeof(grant_entry)) != 0)
            return EPERM;

        if ((grant_entry.flags & (MGF_USED | MGF_VALID)) !=
            (MGF_USED | MGF_VALID))
            return EPERM;

        if (grant_entry.seq != grant_seq) return EPERM;

        if (grant_entry.flags & MGF_INDIRECT) {
            if (depth++ >= MAX_INDIRECT_DEPTH) return ELOOP;

            if (grant_entry.u.indirect.to != grantee && grantee != ANY &&
                grant_entry.u.indirect.to != ANY)
                return EPERM;

            grantee = granter;
            granter = grant_entry.u.indirect.from;
            grant = grant_entry.u.indirect.grant;
        }
    } while (grant_entry.flags & MGF_INDIRECT);

    if ((grant_entry.flags & access) != access) return EPERM;

    if (grant_entry.flags & MGF_DIRECT) {
        if (KERNEL_VMA - grant_entry.u.direct.len + 1 <
            grant_entry.u.direct.addr)
            return EPERM;

        if (grant_entry.u.direct.to != grantee && grantee != ANY &&
            grant_entry.u.direct.to != ANY)
            return EPERM;

        if (offset + bytes < offset ||
            offset + bytes > grant_entry.u.direct.len)
            return EPERM;

        *voffset = grant_entry.u.direct.addr + offset;
        *new_granter = granter;
    } else if (grant_entry.flags & MGF_PROXY) {
        if (!(granter_priv->flags & PRF_ALLOW_PROXY_GRANT)) return EPERM;

        if (grant_entry.u.proxy.to != grantee && grantee != ANY &&
            grant_entry.u.proxy.to != ANY)
            return EPERM;

        if (offset + bytes < offset || offset + bytes > grant_entry.u.proxy.len)
            return EPERM;

        *voffset = grant_entry.u.proxy.addr + offset;
        *new_granter = grant_entry.u.proxy.from;
    } else {
        return EPERM;
    }

    return 0;
}

static int safecopy(struct proc* caller, endpoint_t granter, endpoint_t grantee,
                    mgrant_id_t grant, size_t bytes, vir_bytes offset,
                    vir_bytes addr, int access)
{
    struct vir_addr v_dest, v_src;
    endpoint_t *src, *dest, new_granter;
    vir_bytes voffset;
    int retval;

    if (granter == NO_TASK || grantee == NO_TASK) return EFAULT;

    if (access & MGF_READ) {
        src = &granter;
        dest = &grantee;
    } else {
        src = &grantee;
        dest = &granter;
    }

    if ((retval = verify_grant(granter, grantee, grant, bytes, access, offset,
                               &voffset, &new_granter)) != 0)
        return retval;

    granter = new_granter;

    v_src.proc_ep = *src;
    v_dest.proc_ep = *dest;

    if (access & MGF_READ) {
        v_src.addr = (void*)voffset;
        v_dest.addr = (void*)addr;
    } else {
        v_src.addr = (void*)addr;
        v_dest.addr = (void*)voffset;
    }

    return vir_copy_check(caller, &v_dest, &v_src, bytes);
}

int sys_safecopyfrom(MESSAGE* msg, struct proc* p_proc)
{
    return safecopy(p_proc, msg->u.m_safecopy.src_dest, p_proc->endpoint,
                    msg->u.m_safecopy.grant, msg->u.m_safecopy.len,
                    (vir_bytes)msg->u.m_safecopy.offset,
                    (vir_bytes)msg->u.m_safecopy.addr, MGF_READ);
}

int sys_safecopyto(MESSAGE* msg, struct proc* p_proc)
{
    return safecopy(p_proc, msg->u.m_safecopy.src_dest, p_proc->endpoint,
                    msg->u.m_safecopy.grant, msg->u.m_safecopy.len,
                    (vir_bytes)msg->u.m_safecopy.offset,
                    (vir_bytes)msg->u.m_safecopy.addr, MGF_WRITE);
}
