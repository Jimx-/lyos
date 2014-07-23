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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <sys/stat.h>
#include "page.h"
#include <elf.h>
#include "libexec.h"

struct vfs_exec_info {
    struct exec_info args;
    char prog_name[MAX_PATH];
    struct inode * pin;
    struct vfs_mount * vmnt;
    struct stat sbuf;
};

struct exec_loader {
    libexec_exec_loadfunc_t loader;
};

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf },
    { NULL },
};

PRIVATE int get_exec_inode(struct vfs_exec_info * execi, char * pathname, struct proc * fp);
PRIVATE int read_header(struct vfs_exec_info * execi);
PRIVATE int is_script(struct vfs_exec_info * execi);

/* open the executable and fill in exec info */
PRIVATE int get_exec_inode(struct vfs_exec_info * execi, char * pathname, struct proc * fp)
{
    int retval;

    if (execi->pin) {
        unlock_inode(execi->pin);
        put_inode(execi->pin);
        execi->pin = NULL;
    }

    memcpy(execi->args.prog_name, pathname, MAX_PATH);

    if ((execi->pin = resolve_path(pathname, pcaller)) == NULL) return err_code;
    execi->vmnt = execi->pin->i_vmnt;

    if ((execi->pin->i_mode & I_TYPE) != I_REGULAR) return ENOEXEC;
    if ((retval = forbidden(fp, execi->pin, X_BIT)) != 0) return retval;

    retval = request_stat(execi->pin->i_fs_ep, execi->pin->i_dev, execi->pin->i_num, TASK_FS, (char*)&(execi->sbuf));
    if (retval) return retval;

    if ((retval = read_header(execi)) != 0) return retval;

    return 0;
}

/* read in the executable header */
PRIVATE int read_header(struct vfs_exec_info * execi)
{
    u64 newpos; 
    int bytes_rdwt;
    static char buf[PG_SIZE];

    execi->args.header_len = min(sizeof(buf), execi->pin->i_size);
    execi->args.header = buf;

    /* read the file */
    return request_readwrite(execi->pin->i_fs_ep, execi->pin->i_dev, execi->pin->i_num, 
        0, READ, TASK_FS, buf, execi->args.header_len, &newpos, &bytes_rdwt);
}

/* if there is #!, Not implemented */
PRIVATE int is_script(struct vfs_exec_info * execi)
{
    return 0;
}

/**
 * <Ring 1> Perform the EXEC syscall.
 * @param  msg Ptr to the message.
 * @return     Zero on success.
 */
PUBLIC int do_fs_exec(MESSAGE * msg)
{
    int retval;

    struct vfs_exec_info execi;

    /* get parameters from the message */
    int name_len = msg->NAME_LEN; /* length of filename */
    int src = msg->source;    /* caller proc nr. */
    struct proc * p = proc_table + src;

    memset(&execi, 0, sizeof(execi));

    /* stack info */
    execi.args.stack_top = VM_STACK_TOP;
    execi.args.stack_size = PROC_ORIGIN_STACK;

    /* copy everything we need before we free the old process */
    int orig_stack_len = msg->BUF_LEN;
    if (orig_stack_len > PROC_ORIGIN_STACK) return ENOMEM;  /* stack too big */

    char stackcopy[PROC_ORIGIN_STACK];
    data_copy(TASK_MM, D, stackcopy,
          src, D, mm_msg.BUF,
          orig_stack_len);

    /* copy prog name */
    char pathname[MAX_PATH];
    data_copy(getpid(), D, pathname, src, D, msg->PATHNAME, name_len);
    pathname[name_len] = 0; /* terminate the string */

    retval = get_exec_inode(&execi, pathname, p);
    printl("%d\n", retval);
    if (retval) return retval;

    if (is_script(&execi)) {

    }

    execi.args.proc_e = src;
    execi.args.filesize = execi.pin->i_size;

    int i;
    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi.args);
        /* load successfully */
        if (!retval) break;
    }

    while(1);

}
