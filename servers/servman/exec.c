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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include <stdlib.h>
#include <fcntl.h>
#include "stddef.h"
#include "stdlib.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "proto.h"
#include <sys/stat.h>
#include "page.h"
#include <elf.h>
#include "libexec/libexec.h"
#include <sys/mman.h>
#include <multiboot.h>

#define MAX_MODULE_PARAMS   64

struct exec_loader {
    libexec_exec_loadfunc_t loader;
};

PUBLIC int libexec_load_elf_dbg(struct exec_info * execi);

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf },
    { NULL },
};

PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
    if (offset + len > execi->header_len) return ENOEXEC;
    data_copy(execi->proc_e, (void *)vaddr, SELF, (void *)((int)(execi->header) + offset), len);
    
    return 0;
}

PRIVATE char* prepare_stack(char** argv, size_t* frame_size, int* argc)
{
    char** p;
    size_t stack_size = 2 * sizeof(void*);
    *argc = 0;

    for (p = argv; *p; p++) {
        stack_size = stack_size + sizeof(*p) + strlen(*p) + 1;
        (*argc)++;
    }
    stack_size = (stack_size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    *frame_size = stack_size;

    return (char*) malloc(stack_size);
}

PUBLIC int serv_exec(endpoint_t target, char * exec, int exec_len, char * progname, char** argv)
{
    int i;
    int retval;

    struct exec_info execi;
    memset(&execi, 0, sizeof(execi));

    size_t frame_size;
    int argc;
    char* frame = prepare_stack(argv, &frame_size, &argc);
    if (!frame) return ENOMEM;
    memset(frame, 0, frame_size);

    /* stack info */
    execi.stack_top = VM_STACK_TOP;
    execi.stack_size = PROC_ORIGIN_STACK;

    /* header */
    execi.header = exec;
    execi.header_len = exec_len;

    execi.allocmem = libexec_allocmem;
    execi.allocmem_prealloc = libexec_allocmem_prealloc;
    execi.copymem = read_segment;
    execi.clearproc = libexec_clearproc;
    execi.clearmem = libexec_clearmem;

    execi.proc_e = target;
    execi.filesize = exec_len;

    char* vsp = (char*)VM_STACK_TOP - frame_size;
    char** fpw = (char**) frame;
    char* fp = frame + (sizeof(char*) * argc + 2 * sizeof(void*));

    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi);
        if (!retval) break;  /* loaded successfully */
    }

    if (retval) return retval;

    //int envp_offset;
    char** p;
    for (p = argv; *p; p++) {
        int len = strlen(*p);
        *fpw++ = (char*)(vsp + (fp - frame));
        memcpy(fp, *p, len);
        fp += len;
        *fp++ = '\0';
    }
    *fpw++ = NULL;
    //envp_offset = (char*)fpw - frame;
    *fpw++ = NULL;
    data_copy(target, vsp, SELF, frame, frame_size);
    free(frame);

    struct ps_strings ps;
    ps.ps_nargvstr = argc;
    ps.ps_argvstr = vsp;
    //ps.ps_envstr = vsp + envp_offset;
    ps.ps_envstr = NULL;

    return kernel_exec(target, vsp, progname, (void*) execi.entry_point, &ps);
}


#if 0
PRIVATE int module_argc;
PRIVATE char module_stack[PROC_ORIGIN_STACK];
PRIVATE char * module_stp;
PRIVATE char * module_envp;
PRIVATE int module_stack_len;

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

    //CLEAR_ARG();
    //sprintf(arg, "initrd_base=0x%x", (unsigned int)initrd_base);
    //COPY_STRING(arg);

    //CLEAR_ARG();
    //sprintf(arg, "initrd_len=%u", initrd_len);
    //COPY_STRING(arg);

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

#endif
