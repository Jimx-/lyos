/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdio.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <sys/syslimits.h>
#include <errno.h>
#include <lyos/fs.h>
#include <libsysfs/libsysfs.h>

#include "types.h"
#include "proto.h"
#include "global.h"

int do_mapdriver(void)
{
    endpoint_t src = self->msg_in.source;
    void* label_user = self->msg_in.u.m_vfs_mapdriver.label;
    int label_len = self->msg_in.u.m_vfs_mapdriver.label_len;
    void* domains_user = self->msg_in.u.m_vfs_mapdriver.domains;
    int nr_domains = self->msg_in.u.m_vfs_mapdriver.nr_domains;
    char label[PROC_NAME_LEN + 1];
    char endpoint_label[PATH_MAX];
    endpoint_t driver_ep;
    struct fproc* fp;
    int domains[NR_DOMAIN];
    int retval;

    if (label_len > PROC_NAME_LEN) return ENAMETOOLONG;

    retval = data_copy(SELF, label, src, label_user, label_len);
    if (retval) return retval;
    label[label_len] = '\0';

    snprintf(endpoint_label, sizeof(endpoint_label),
             SYSFS_SERVICE_ENDPOINT_LABEL, label);
    retval = sysfs_retrieve_u32(endpoint_label, (u32*)&driver_ep);
    if (retval) return retval;

    if (!(fp = vfs_endpt_proc(driver_ep))) return EINVAL;
    fp->flags |= FPF_DRV_PROC;

    if (nr_domains > 0) {
        if (nr_domains > NR_DOMAIN) {
            return EINVAL;
        }

        retval = data_copy(SELF, domains, src, domains_user,
                           sizeof(int) * nr_domains);
        if (retval) return retval;

        retval = sdev_mapdriver(label, driver_ep, domains, nr_domains);
        if (retval) return retval;
    }

    return 0;
}
