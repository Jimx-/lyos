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
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <sys/stat.h>
#include <sys/syslimits.h>
#include "page.h"
#include <lyos/vm.h>
#include <elf.h>
#include <lyos/sysutils.h>
#include "libexec/libexec.h"
#include <sys/mman.h>

struct vfs_exec_info {
    struct exec_info args;
    char prog_name[MAX_PATH];
    char dyn_prog_name[MAX_PATH];
    struct inode * pin;
    struct vfs_mount * vmnt;
    struct stat sbuf;
    int mmfd;
    int exec_fd;
    /* dynamic linking */
    int is_dyn;
    int dyn_phnum;
    void* dyn_phdr;
    void* dyn_entry;
};

typedef int (*stack_hook_t)(struct vfs_exec_info* execi, char* stack, size_t* stack_size, void** vsp);
PRIVATE int setup_stack_elf32(struct vfs_exec_info* execi, char* stack, size_t* stack_size, void** vsp);
PRIVATE int setup_script_stack(struct inode* pin, char* stack, size_t* stack_size, char* pathname, void** vsp);
PRIVATE int prepend_arg(int replace, char* stack, size_t* stack_size, char* arg, void** vsp);
struct exec_loader {
    libexec_exec_loadfunc_t loader;
    stack_hook_t setup_stack;
};

PUBLIC int libexec_load_elf_dbg(struct exec_info * execi);

PRIVATE struct exec_loader exec_loaders[] = {
    { libexec_load_elf, setup_stack_elf32 },
    { NULL },
};

PRIVATE int get_exec_inode(struct vfs_exec_info * execi, struct lookup* lookup, struct fproc * fp);
PRIVATE int read_header(struct vfs_exec_info * execi);
PRIVATE int is_script(struct vfs_exec_info * execi);
PRIVATE int request_vfs_mmap(struct exec_info *execi,
    void* vaddr, size_t len, off_t foffset, int protflags, size_t clearend);

/* open the executable and fill in exec info */
PRIVATE int get_exec_inode(struct vfs_exec_info * execi, struct lookup* lookup, struct fproc * fp)
{
    int retval;

    if (execi->pin) {
        unlock_inode(execi->pin);
        put_inode(execi->pin);
        execi->pin = NULL;
    }

    memcpy(execi->prog_name, lookup->pathname, MAX_PATH);
    memcpy(execi->args.prog_name, lookup->pathname, MAX_PATH);

    if ((execi->pin = resolve_path(lookup, fp)) == NULL) return err_code;
    execi->vmnt = execi->pin->i_vmnt;
    unlock_vmnt(execi->vmnt);

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
    size_t bytes_rdwt;
    static char buf[PG_SIZE];

    execi->args.header_len = min(sizeof(buf), execi->pin->i_size);
    execi->args.header = buf;

    /* read the file */
    return request_readwrite(execi->pin->i_fs_ep, execi->pin->i_dev, execi->pin->i_num,
        0, READ, TASK_FS, buf, execi->args.header_len, &newpos, &bytes_rdwt);
}

/* read segment */
PRIVATE int read_segment(struct exec_info *execi, off_t offset, void* vaddr, size_t len)
{
    u64 newpos;
    size_t bytes_rdwt;

    struct vfs_exec_info * vexeci = (struct vfs_exec_info *)(execi->callback_data);
    struct inode * pin = vexeci->pin;

    if (offset + len > pin->i_size){
        len = pin->i_size - offset;
        //return EIO;
    }

    return request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num,
        (u64)offset, READ, execi->proc_e, vaddr, len, &newpos, &bytes_rdwt);
}

PRIVATE int is_script(struct vfs_exec_info * execi)
{
    return ((execi->args.header[0] == '#') && (execi->args.header[1] == '!'));
}

/* issue a vfs_mmap request */
PRIVATE int request_vfs_mmap(struct exec_info *execi, void* vaddr, size_t len, off_t foffset, int protflags, size_t clearend)
{
    struct vfs_exec_info * vexeci = (struct vfs_exec_info *)(execi->callback_data);
    struct inode * pin = vexeci->pin;

    int flags = MAP_PRIVATE | MAP_FIXED;

    return vfs_mmap(execi->proc_e, foffset, len, pin->i_dev, pin->i_num, vexeci->mmfd, vaddr, flags, protflags, clearend);
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
    struct fproc * mm_task = vfs_endpt_proc(TASK_MM);
    int i;

    memset(&execi, 0, sizeof(execi));
    lock_fproc(mm_task);

    /* stack info */
    execi.args.stack_top = (void*) VM_STACK_TOP;
    execi.args.stack_size = PROC_ORIGIN_STACK;

    /* uid & gid */
    execi.args.new_uid = fp->realuid;
    execi.args.new_gid = fp->realgid;
    execi.args.new_euid = fp->effuid;
    execi.args.new_egid = fp->effgid;

    execi.is_dyn = 0;
    execi.exec_fd = -1;

    /* copy everything we need before we free the old process */
    void* user_sp = msg->BUF;
    size_t orig_stack_len = msg->BUF_LEN;
    if (orig_stack_len > PROC_ORIGIN_STACK) {
        retval = ENOMEM;  /* stack too big */
        goto exec_finalize;
    }

    static char stackcopy[PROC_ORIGIN_STACK];
    memset(stackcopy, 0, sizeof(stackcopy));
    data_copy(SELF, stackcopy,
          src, msg->BUF,
          orig_stack_len);

    /* copy prog name */
    char pathname[PATH_MAX];
    data_copy(SELF, pathname, src, msg->PATHNAME, name_len);
    pathname[name_len] = 0; /* terminate the string */

    struct lookup lookup;
    init_lookup(&lookup, pathname, 0, &execi.vmnt, &execi.pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_READ;
    retval = get_exec_inode(&execi, &lookup, fp);
    if (retval) goto exec_finalize;

    /* relocate stack pointers */
    char* orig_stack = (char*)(VM_STACK_TOP - orig_stack_len);
    int delta = (void*) orig_stack - user_sp;

    if (orig_stack_len) {
        char **q = (char**)stackcopy;
        for (; *q != 0; q++) {
            *q += delta;
        }
        q++;
        for (; *q != 0; q++)
            *q += delta;
    }

    if (is_script(&execi)) {
        setup_script_stack(execi.pin, stackcopy, &orig_stack_len, pathname, (void**) &orig_stack);
        retval = get_exec_inode(&execi, &lookup, fp);
        if (retval) goto exec_finalize;
    }

    /* find an fd for MM */
    /* find a free slot in PROCESS::filp[] */
    int fd;
    struct file_desc * filp = NULL;
    retval = get_fd(mm_task, 0, &fd, &filp);
    if (retval) return retval;

    if (!filp || fd < 0) {
        execi.mmfd = -1;
        execi.args.memmap = NULL;
    } else {
        mm_task->filp[fd] = filp;
        filp->fd_cnt = 1;
        filp->fd_pos = 0;
        filp->fd_inode = execi.pin;
        filp->fd_inode->i_cnt++;
        filp->fd_mode = O_RDONLY;
        execi.mmfd = fd;
        execi.args.memmap = request_vfs_mmap;
    }

    execi.args.allocmem = libexec_allocmem;
    execi.args.allocmem_prealloc = libexec_allocmem_prealloc;
    execi.args.copymem = read_segment;
    execi.args.clearproc = libexec_clearproc;
    execi.args.clearmem = libexec_clearmem;
    execi.args.callback_data = (void *)&execi;

    execi.args.proc_e = src;
    execi.args.filesize = execi.pin->i_size;

    char interp[PATH_MAX];
    if (elf_is_dynamic(execi.args.header, execi.args.header_len, interp, sizeof(interp)) > 0) {
        execi.exec_fd = common_open(fp, pathname, O_RDONLY, 0);
        if (execi.exec_fd < 0) {
            retval = -execi.exec_fd;
            goto exec_finalize;
        }

        /* load the main executable first */
        for (i = 0; exec_loaders[i].loader != NULL; i++) {
            retval = (*exec_loaders[i].loader)(&execi.args);
            if (!retval) break;  /* loaded successfully */
        }
        if (retval) goto exec_finalize;

        execi.is_dyn = 1;
        execi.dyn_entry = execi.args.entry_point;
        execi.dyn_phdr = execi.args.phdr;
        execi.dyn_phnum = execi.args.phnum;
        strlcpy((char*) execi.dyn_prog_name, (char*)execi.prog_name, sizeof(execi.dyn_prog_name));

        init_lookup(&lookup, interp, 0, &execi.vmnt, &execi.pin);
        retval = get_exec_inode(&execi, &lookup, fp);
        if (retval) goto exec_finalize;

        execi.args.filesize = execi.pin->i_size;
        execi.args.clearproc = NULL;
        execi.args.memmap = NULL;
        /* prevent libexec from allocating stack twice */
        execi.args.allocmem_prealloc = NULL;
    }


    for (i = 0; exec_loaders[i].loader != NULL; i++) {
        retval = (*exec_loaders[i].loader)(&execi.args);
        if (!retval) {
            if (exec_loaders[i].setup_stack) retval = (*exec_loaders[i].setup_stack)(&execi, stackcopy, &orig_stack_len, (void**) &orig_stack);
            break;  /* loaded successfully */
        }
    }
    if (retval) goto exec_finalize;

    int argc = 0;
    char * envp = orig_stack;
    if (orig_stack_len) {
        char **q = (char**)stackcopy;
        for (; *q != 0; q++, argc++) ;
        q++;
        envp += (void*) q - (void*) stackcopy;
    }

    data_copy(src, orig_stack, SELF, stackcopy, orig_stack_len);

    struct ps_strings ps;
    ps.ps_nargvstr = argc;
    ps.ps_argvstr = orig_stack;
    ps.ps_envstr = envp;

    /* record frame info */
    msg->BUF = orig_stack;
    msg->BUF_LEN = orig_stack_len;

exec_finalize:
    if (filp) {
        unlock_filp(filp);
    }

    if (execi.pin) {
        unlock_inode(execi.pin);
        put_inode(execi.pin);
    }

    unlock_fproc(mm_task);

    if (!retval) {
        return kernel_exec(src, orig_stack, pathname, (void*) execi.args.entry_point, &ps);
    }

    return retval;
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
PRIVATE int setup_stack_elf32(struct vfs_exec_info* execi, char* stack, size_t* stack_size, void** vsp)
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
        vec->a_un.a_val = (typeof(vec->a_un.a_val)) val; \
        vec++; \
    } else { \
        vec--; \
        vec->a_type = AT_NULL; \
        vec->a_un.a_val = 0; \
        vec++; \
    }

    if (!execi->is_dyn) {
        AUXV_ENT(auxv, AT_ENTRY, execi->args.entry_point);
        AUXV_ENT(auxv, AT_PHDR, execi->args.phdr);
        AUXV_ENT(auxv, AT_PHNUM, execi->args.phnum);
    } else {
        AUXV_ENT(auxv, AT_ENTRY, execi->dyn_entry);
        AUXV_ENT(auxv, AT_PHDR, execi->dyn_phdr);
        AUXV_ENT(auxv, AT_PHNUM, execi->dyn_phnum);
    }
    AUXV_ENT(auxv, AT_BASE, (u32)execi->args.load_base);
    AUXV_ENT(auxv, AT_EXECFD, execi->exec_fd);
    AUXV_ENT(auxv, AT_UID, execi->args.new_uid);
    AUXV_ENT(auxv, AT_GID, execi->args.new_gid);
    AUXV_ENT(auxv, AT_EUID, execi->args.new_euid);
    AUXV_ENT(auxv, AT_EGID, execi->args.new_egid);
    AUXV_ENT(auxv, AT_PAGESZ, PG_SIZE);
    AUXV_ENT(auxv, AT_CLKTCK, get_system_hz());
    AUXV_ENT(auxv, AT_SYSINFO, (void*) sysinfo);

    Elf32_auxv_t* auxv_execfn = auxv;
    AUXV_ENT(auxv, AT_EXECFN, NULL);
    AUXV_ENT(auxv, AT_PLATFORM, NULL);
    AUXV_ENT(auxv, AT_NULL, 0);

    char* prog_name = execi->is_dyn ? (char*) execi->dyn_prog_name : (char*) execi->prog_name;
    size_t name_len = strlen(prog_name) + 1 + strlen(LYOS_PLATFORM) + 1;
    char* userp = NULL;
    /* add AT_EXECFN and AT_PLATFORM if there is enough space */
    if ((void*)auxv_end - (void*)auxv > name_len) {
        strcpy((char*)auxv, prog_name);
        strcpy((char*)auxv + strlen(prog_name) + 1, LYOS_PLATFORM);
        auxv_end = (Elf32_auxv_t*)((void*)auxv + name_len);
        auxv_end = (Elf32_auxv_t*)roundup((u32)auxv_end, sizeof(int));
        userp = (char*) ((void*)auxv - (void*)auxv_buf);
        userp += (uintptr_t) *vsp + (uintptr_t)arg_str - (uintptr_t)stack;
    }

    size_t auxv_len = (void*)auxv_end - (void*)auxv_buf;
    if (userp) {
        userp -= auxv_len;
        AUXV_ENT(auxv_execfn, AT_EXECFN, userp);
        AUXV_ENT(auxv_execfn, AT_PLATFORM, userp + strlen(prog_name) + 1);
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

PRIVATE int setup_script_stack(struct inode* pin, char* stack, size_t* stack_size, char* pathname, void** vsp)
{
    prepend_arg(1, stack, stack_size, pathname, vsp);

    u64 newpos;
    int retval;
    char buf[PG_SIZE];
    size_t bytes_rdwt;
    char* interp = NULL;

    /* read the file */
    retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num,
        0, READ, TASK_FS, buf, sizeof(buf), &newpos, &bytes_rdwt);
    if (retval) return retval;

    int n = pin->i_size;
    if (n > sizeof(buf)) n = sizeof(buf);
    n -= 2;
    char* p = &buf[2];
    if (n > PATH_MAX) n = PATH_MAX;

    memcpy(pathname, p, n);
    p = memchr(pathname, '\n', n);
    if (!p) return ENOEXEC;
    p--;

    while (TRUE) {
        while (p > pathname && (*p == ' ' || *p == '\t')) p--;
        if (p <= pathname) break;
        *(p+1) = '\0';

        while (p > pathname && (*p != ' ' && *p != '\t')) p--;
        if (p != pathname) p++;
        interp = p;
        p--;

        prepend_arg(0, stack, stack_size, interp, vsp);
    }

    if (!interp) return ENOEXEC;
    if (interp != pathname) memmove(pathname, interp, strlen(interp) + 1);

    return 0;
}

PRIVATE int prepend_arg(int replace, char* stack, size_t* stack_size, char* arg, void** vsp)
{
    int arglen = strlen(arg) + 1;
    char** arg0 = (char**)stack;
    /* delta of argv[0] to vsp */
    int delta = (int) ((void*) *arg0 - *vsp);
    int orig_size = *stack_size;
    char* arg_start;

    int offset;
    if (replace) {
        /* replace the former argument with the new one */
        arglen--;
        offset = arglen - strlen(stack + delta);
        arg_start = stack + delta;
    } else {
        /* insert a pointer and a string */
        offset = arglen + sizeof(char*);
        arg_start = stack + delta + sizeof(char*);
    }

    if (*stack_size + offset >= ARG_MAX) {
        return ENOMEM;
    }
    memmove(stack + delta + offset, stack + delta, orig_size - delta);
    memcpy(arg_start, arg, arglen);

    if (!replace) {
        memmove(stack + sizeof(char*), stack, delta);
    }

    *arg0 = (char*) (*vsp + delta - (replace ? offset : arglen));
    *stack_size += offset;
    *vsp -= offset;

    return 0;
}
