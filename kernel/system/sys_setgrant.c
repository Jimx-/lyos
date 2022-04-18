#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <kernel/proc.h>
#include <lyos/const.h>
#include <lyos/priv.h>
#include <errno.h>

int sys_setgrant(MESSAGE* m, struct proc* p_proc)
{
    struct priv* priv = p_proc->priv;

    if (!priv || !(priv->flags & PRF_PRIV_PROC)) {
        return EPERM;
    } else {
        priv->grant_table = (vir_bytes)m->BUF;
        priv->grant_entries = m->CNT;
        priv->grant_endpoint = p_proc->endpoint;
    }

    return 0;
}
