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
#include <fcntl.h>
#include "stddef.h"
#include "stdlib.h"
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
#include "proto.h"
#include <sys/stat.h>
#include "page.h"
#include <elf.h>
#include "libexec.h"
#include <sys/mman.h>

struct exec_loader {
    libexec_exec_loadfunc_t loader;
};

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf },
    { NULL },
};

PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;
    data_copy(execi->proc_e, D, (void *)vaddr, TASK_SERVMAN, D, (void *)((int)(execi->header) + offset), len);
    
    return 0;
}

PUBLIC int serv_exec(endpoint_t target, char * pathname)
{
    int i;

    int exec_fd = open(pathname, O_RDONLY);
    if (exec_fd == -1) return ENOENT;

    struct stat sbuf;
    int retval = fstat(exec_fd, &sbuf);
    if (retval) return retval;

    char * buf = (char*)malloc(sbuf.st_size);
    if (buf == NULL) return ENOMEM;

    int n_read = read(exec_fd, buf, sbuf.st_size);
    if (n_read != sbuf.st_size) return ENOEXEC;

    close(exec_fd);

    struct exec_info execi;
    memset(&execi, 0, sizeof(execi));

    /* stack info */
    execi.stack_top = VM_STACK_TOP;
    execi.stack_size = PROC_ORIGIN_STACK;

    /* header */
    execi.header = buf;
    execi.header_len = sbuf.st_size;

    execi.allocmem = libexec_allocmem;
    execi.allocstack = libexec_allocstack;
    execi.alloctext = libexec_alloctext;
    execi.copymem = read_segment;
    execi.clearproc = libexec_clearproc;
    execi.clearmem = libexec_clearmem;

    execi.proc_e = target;
    execi.filesize = sbuf.st_size;

    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi);
        if (!retval) break;  /* loaded successfully */
    }

    if (retval) return retval;

    /* setup eip & esp */
    proc_table[target].regs.eip = execi.entry_point; /* @see _start.asm */
    proc_table[target].regs.esp = (u32)VM_STACK_TOP;

    proc_table[target].brk = execi.brk;
    strcpy(proc_table[target].name, pathname);

    return 0;
}
