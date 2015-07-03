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
#include <lyos/compile.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include <fcntl.h>
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <sys/stat.h>
#include "page.h"
#include <elf.h>
#include <lyos/sysutils.h>
#include "libexec/libexec.h"
#include <sys/mman.h>

struct vfs_exec_info {
    struct exec_info args;
    char prog_name[MAX_PATH];
    struct inode * pin;
    struct vfs_mount * vmnt;
    struct stat sbuf;
    int mmfd;
};

typedef int (*stack_hook_t)(struct vfs_exec_info* execi, char* stack, size_t* stack_size, vir_bytes* vsp);
PRIVATE int setup_stack_elf32(struct vfs_exec_info* execi, char* stack, size_t* stack_size, vir_bytes* vsp);
struct exec_loader {
    libexec_exec_loadfunc_t loader;
    stack_hook_t setup_stack;
};

PUBLIC int libexec_load_elf_dbg(struct exec_info * execi);

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf, setup_stack_elf32 },
    { NULL },
};

PRIVATE int get_exec_inode(struct vfs_exec_info * execi, char * pathname, struct fproc * fp);
PRIVATE int read_header(struct vfs_exec_info * execi);
PRIVATE int is_script(struct vfs_exec_info * execi);
PRIVATE int request_vfs_mmap(struct exec_info *execi,
    int vaddr, int len, int foffset, int protflags);

/* open the executable and fill in exec info */
PRIVATE int get_exec_inode(struct vfs_exec_info * execi, char * pathname, struct fproc * fp)
{
    int retval;

    if (execi->pin) {
        unlock_inode(execi->pin);
        put_inode(execi->pin);
        execi->pin = NULL;
    }

    memcpy(execi->prog_name, pathname, MAX_PATH);
    memcpy(execi->args.prog_name, pathname, MAX_PATH);

    if ((execi->pin = resolve_path(pathname, fp)) == NULL) return err_code;
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

/* read segment */
PRIVATE int read_segment(struct exec_info *execi, off_t offset, int vaddr, size_t len)
{
    u64 newpos; 
    int bytes_rdwt;

    struct vfs_exec_info * vexeci = (struct vfs_exec_info *)(execi->callback_data);
    struct inode * pin = vexeci->pin;

    if (offset + len > pin->i_size){
        len = pin->i_size - offset;
        //return EIO;
    }

    return request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, 
        (u64)offset, READ, execi->proc_e, (char *)vaddr, len, &newpos, &bytes_rdwt);
}

/* if there is #!, Not implemented */
PRIVATE int is_script(struct vfs_exec_info * execi)
{
    return ((execi->args.header[0] == '#') && (execi->args.header[1] == '!'));
}

/* issue a vfs_mmap request */
PRIVATE int request_vfs_mmap(struct exec_info *execi,
    int vaddr, int len, int foffset, int protflags)
{
    struct vfs_exec_info * vexeci = (struct vfs_exec_info *)(execi->callback_data);
    struct inode * pin = vexeci->pin;

    int flags = 0;

    return vfs_mmap(execi->proc_e, foffset, len, pin->i_dev, pin->i_num, vexeci->mmfd, vaddr, flags);
}

/*****************************************************************************
 *                            do_exec
 *****************************************************************************/
/**
 * <Ring 3> Perform the EXEC syscall.
 * 
 * @param  msg Ptr to the message.
 * @return     Zero on success.
 * 
 *****************************************************************************/
PUBLIC int fs_exec(MESSAGE * msg)
{
    int retval;

    struct vfs_exec_info execi;

    /* get parameters from the message */
    int name_len = msg->NAME_LEN; /* length of filename */
    int src = msg->ENDPOINT;    /* caller proc nr. */
    struct fproc * fp = vfs_endpt_proc(src);
    int i;

    memset(&execi, 0, sizeof(execi));

    /* stack info */
    execi.args.stack_top = VM_STACK_TOP;
    execi.args.stack_size = PROC_ORIGIN_STACK;

    /* uid & gid */
    execi.args.new_uid = fp->realuid;
    execi.args.new_gid = fp->realgid;
    execi.args.new_euid = fp->effuid;
    execi.args.new_egid = fp->effgid;

    /* copy everything we need before we free the old process */
    int orig_stack_len = msg->BUF_LEN;
    if (orig_stack_len > PROC_ORIGIN_STACK) return ENOMEM;  /* stack too big */

    static char stackcopy[PROC_ORIGIN_STACK];
    memset(stackcopy, 0, sizeof(stackcopy));
    data_copy(SELF, stackcopy,
          src, msg->BUF,
          orig_stack_len);

    /* copy prog name */
    char pathname[MAX_PATH];
    data_copy(SELF, pathname, src, msg->PATHNAME, name_len);
    pathname[name_len] = 0; /* terminate the string */

    retval = get_exec_inode(&execi, pathname, fp);
    if (retval) return retval;

    if (is_script(&execi)) {
        printl("Is a script!\n");
        while(1);
    }

    char interp[MAX_PATH];
    if (elf_is_dynamic(execi.args.header, execi.args.header_len, interp, sizeof(interp)) > 0) {
        printl("%s\n", interp);
    }

    /* find an fd for MM */
    struct fproc * mm_task = vfs_endpt_proc(TASK_MM);
    /* find a free slot in PROCESS::filp[] */
    int fd = -1;
    for (i = 0; i < NR_FILES; i++) {
        if (mm_task->filp[i] == 0) {
            fd = i;
            break;
        }
    }
    if (fd < 0)
        panic("VFS: do_exec(): MM's filp[] is full");

    for (i = 0; i < NR_FILE_DESC; i++)
        if (f_desc_table[i].fd_inode == 0)
            break;
    if (i >= NR_FILE_DESC)
        panic("f_desc_table[] is full.");

    struct file_desc * filp = &f_desc_table[i];
    mm_task->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_pos = 0;
    filp->fd_inode = execi.pin;
    filp->fd_mode = O_RDONLY;
    execi.mmfd = fd;
    execi.args.memmap = request_vfs_mmap;

    execi.args.allocmem = libexec_allocmem;
    execi.args.allocmem_prealloc = libexec_allocmem_prealloc;
    execi.args.copymem = read_segment;
    execi.args.clearproc = libexec_clearproc;
    execi.args.clearmem = libexec_clearmem;
    execi.args.callback_data = (void *)&execi;

    execi.args.proc_e = src;
    execi.args.filesize = execi.pin->i_size;

    /* relocate stack pointers */
    char * orig_stack = (char*)(VM_STACK_TOP - orig_stack_len);

    int delta = (int)orig_stack - (int)msg->BUF;

    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi.args);
        if (!retval) {
            if (exec_loaders[i].setup_stack) retval = (*exec_loaders[i].setup_stack)(&execi, stackcopy, (size_t*)&orig_stack_len, (vir_bytes*)&orig_stack);
            break;  /* loaded successfully */
        }
    }

    if (retval) return retval;

    int argc = 0;
    char * envp = orig_stack;

    if (orig_stack_len) {  
        char **q = (char**)stackcopy;
        for (; *q != 0; q++, argc++) {
            *q += delta;
        }
        q++;
        envp += (int)q - (int)stackcopy;
        for (; *q != 0; q++)
            *q += delta;
    } 

    data_copy(src, orig_stack, TASK_FS, stackcopy, orig_stack_len);

    struct ps_strings ps;
    ps.ps_nargvstr = argc;
    ps.ps_argvstr = orig_stack;
    ps.ps_envstr = envp;

    return kernel_exec(src, orig_stack, pathname, execi.args.entry_point, &ps);
}

/*****************************************************************************
 *                        setup_stack_elf32
 *****************************************************************************/
/**                   
 * <Ring 3> Setup auxiliary vectors.
 * 
 * @param vfs_exec_info Ptr to exec info.
 * @param stack Stack pointer.
 * @param stack_size Size of stack.
 * @param vsp Virtual stack pointer in caller's address space.
 * 
 * @return Zero on success.
 *****************************************************************************/
PRIVATE int setup_stack_elf32(struct vfs_exec_info* execi, char* stack, size_t* stack_size, vir_bytes* vsp)
{
    char** arg_str;
    if (*stack_size) {  
        arg_str = (char**)stack;
        for (; *arg_str != 0; arg_str++) ;
        arg_str++;
        for (; *arg_str != 0; arg_str++) ;
        arg_str++;
    }

    size_t strings_len = stack + *stack_size - (char*)arg_str;

    static char auxv_buf[PROC_ORIGIN_STACK];
    memset(auxv_buf, 0, sizeof(auxv_buf));

    Elf32_auxv_t* auxv = (Elf32_auxv_t*) auxv_buf;
    Elf32_auxv_t* auxv_end = (Elf32_auxv_t*) (auxv_buf + sizeof(auxv_buf));
#define AUXV_ENT(vec, type, val) \
    if (vec < auxv_end) { \
        vec->a_type = type; \
        vec->a_un.a_val = val; \
        vec++; \
    } else { \
        vec--; \
        vec->a_type = AT_NULL; \
        vec->a_un.a_val = 0; \
        vec++; \
    }

    AUXV_ENT(auxv, AT_ENTRY, execi->args.entry_point);
    AUXV_ENT(auxv, AT_UID, execi->args.new_uid);
    AUXV_ENT(auxv, AT_GID, execi->args.new_gid);
    AUXV_ENT(auxv, AT_EUID, execi->args.new_euid);
    AUXV_ENT(auxv, AT_EGID, execi->args.new_egid);
    AUXV_ENT(auxv, AT_PAGESZ, PG_SIZE);
    AUXV_ENT(auxv, AT_CLKTCK, get_system_hz());

    Elf32_auxv_t* auxv_execfn = auxv;
    AUXV_ENT(auxv, AT_EXECFN, NULL);
    AUXV_ENT(auxv, AT_PLATFORM, NULL);
    AUXV_ENT(auxv, AT_NULL, 0);

    size_t name_len = strlen(execi->prog_name) + 1 + strlen(LYOS_PLATFORM) + 1;
    char* userp = NULL;
    /* add AT_EXECFN and AT_PLATFORM if there is enough space */
    if ((vir_bytes)auxv_end - (vir_bytes)auxv > name_len) {
        strcpy((char*)auxv, execi->prog_name);
        strcpy((char*)auxv + strlen(execi->prog_name) + 1, LYOS_PLATFORM);
        auxv_end = (Elf32_auxv_t*)((vir_bytes)auxv + name_len); 
        auxv_end = (Elf32_auxv_t*)roundup((u32)auxv_end, sizeof(int));
        userp = (vir_bytes)auxv - (vir_bytes)auxv_buf;
        userp += *vsp + (vir_bytes)arg_str - (vir_bytes)stack; 
    }

    size_t auxv_len = (vir_bytes)auxv_end - (vir_bytes)auxv_buf;
    if (userp) {
        userp -= auxv_len;
        AUXV_ENT(auxv_execfn, AT_EXECFN, userp);
        AUXV_ENT(auxv_execfn, AT_PLATFORM, userp + strlen(execi->prog_name) + 1);
    } else {
        /* AT_EXECFN and AT_PLATFORM not present */
        AUXV_ENT(auxv_execfn, AT_NULL, 0);
    }
    memmove((char*)arg_str + auxv_len, (char*)arg_str, strings_len);
    memcpy((char*)arg_str, auxv_buf, auxv_len);

    *stack_size = *stack_size + auxv_len;
    *vsp = *vsp - auxv_len;

    return 0;
}
