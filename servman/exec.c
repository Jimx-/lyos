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
#include <multiboot.h>

#define MAX_MODULE_PARAMS   64

struct exec_loader {
    libexec_exec_loadfunc_t loader;
};

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf },
    { NULL },
};

PRIVATE int module_argc;
PRIVATE char module_stack[PROC_ORIGIN_STACK];
PRIVATE char * module_stp;
PRIVATE char * module_envp;
PRIVATE int module_stack_len;

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

PUBLIC int serv_prepare_module_stack()
{
    char arg[STR_DEFAULT_LEN];
    char * argv[MAX_MODULE_PARAMS];
    char * stacktop = module_stack + PROC_ORIGIN_STACK, ** arg_list;
    int delta = (int)stacktop - VM_STACK_TOP;
    int i;

#define CLEAR_ARG() memset(arg, 0, sizeof(arg))
#define COPY_STRING(str) do { \
                                stacktop--;    \
                                stacktop -= strlen(str);   \
                                strcpy(stacktop, str); \
                                argv[module_argc] = stacktop - delta; \
                                module_argc++;  \
                            } while (0)

    module_argc = 0;

    /* initrd_base and initrd_len */
    multiboot_module_t * initrd_mod = (multiboot_module_t *)mb_mod_addr;
    char * initrd_base = (char*)(initrd_mod->mod_start + KERNEL_VMA);
    unsigned int initrd_len = initrd_mod->mod_end - initrd_mod->mod_start;

    CLEAR_ARG();
    sprintf(arg, "%x", (unsigned int)initrd_base);
    COPY_STRING(arg);
    COPY_STRING("--initrd_base");

    CLEAR_ARG();
    sprintf(arg, "%u", initrd_len);
    COPY_STRING(arg);
    COPY_STRING("--initrd_len");

    arg_list = (char **)stacktop;
    /* env list end */
    arg_list--;
    *arg_list = NULL;
    module_envp = (char*)arg_list - delta;
    /* arg list end */
    arg_list--;
    *arg_list = NULL;

    for (i = 0; i < module_argc; i++) {
        arg_list--;
        *arg_list = argv[i];
    }

    module_stp = (char*)arg_list;
    module_stack_len = (int)module_stack + PROC_ORIGIN_STACK - (int)arg_list;

    return 0;
}

PUBLIC int serv_spawn_module(endpoint_t target, char * mod_base, u32 mod_len)
{
    int i, retval;

    struct exec_info execi;
    memset(&execi, 0, sizeof(execi));

    /* stack info */
    execi.stack_top = VM_STACK_TOP;
    execi.stack_size = PROC_ORIGIN_STACK;

    /* header */
    execi.header = mod_base;
    execi.header_len = mod_len;

    execi.allocmem = libexec_allocmem;
    execi.allocstack = libexec_allocstack;
    execi.alloctext = libexec_alloctext;
    execi.copymem = read_segment;
    execi.clearproc = libexec_clearproc;
    execi.clearmem = libexec_clearmem;

    execi.proc_e = target;
    execi.filesize = mod_len;

    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi);
        if (!retval) break;  /* loaded successfully */
    }

    if (retval) return retval;

    char * orig_stack = (char*)(VM_STACK_TOP - module_stack_len);
    data_copy(target, D, orig_stack, TASK_SERVMAN, D, module_stp, module_stack_len);

    proc_table[target].regs.ecx = (u32)module_envp; 
    proc_table[target].regs.edx = (u32)orig_stack;
    proc_table[target].regs.eax = module_argc;

    /* setup eip & esp */
    proc_table[target].regs.eip = execi.entry_point; /* @see _start.asm */
    proc_table[target].regs.esp = (u32)VM_STACK_TOP - module_stack_len;

    proc_table[target].brk = execi.brk;

    return 0;
}
