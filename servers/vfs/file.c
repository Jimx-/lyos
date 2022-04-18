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
#include <lyos/sysutils.h>
#include <stdlib.h>
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "errno.h"
#include <lyos/fs.h>
#include <fcntl.h>

#include "types.h"
#include "proto.h"
#include "global.h"

static ssize_t vfs_read(struct file_desc* filp, char* buf, size_t count,
                        loff_t* ppos, struct fproc* fp)
{
    size_t bytes = 0;
    int retval;
    struct inode* pin = filp->fd_inode;

    retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, *ppos,
                               READ /* rw_flag */, fp->endpoint, buf, count,
                               ppos, &bytes);

    if (retval) {
        return -retval;
    }

    return bytes;
}

static ssize_t vfs_write(struct file_desc* filp, const char* buf, size_t count,
                         loff_t* ppos, struct fproc* fp)
{
    size_t bytes = 0;
    int retval;
    struct inode* pin = filp->fd_inode;
    loff_t position = *ppos;

    /* check for O_APPEND */
    if (filp->fd_flags & O_APPEND) position = pin->i_size;

    retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, position,
                               WRITE /* rw_flag */, fp->endpoint, buf, count,
                               ppos, &bytes);

    if (retval) {
        return -retval;
    }

    return bytes;
}

const struct file_operations vfs_fops = {
    .read = vfs_read,
    .write = vfs_write,
};

void lock_filp(struct file_desc* filp, rwlock_type_t lock_type)
{
    int retval;
    struct worker_thread* old_self;

    if (filp->fd_inode) lock_inode(filp->fd_inode, lock_type);

    retval = mutex_trylock(&filp->fd_lock);
    if (retval == 0) {
        return;
    }

    /* need blocking */
    old_self = worker_suspend(WT_BLOCKED_ON_LOCK);

    retval = mutex_lock(&filp->fd_lock);
    if (retval != 0) {
        panic("failed to acquire filp lock: %d", retval);
    }

    worker_resume(old_self);
}

void unlock_filp(struct file_desc* filp)
{
    int retval;

    if (filp->fd_cnt > 0) {
        unlock_inode(filp->fd_inode);
    }

    retval = mutex_unlock(&filp->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }
}

void unlock_filps(struct file_desc* filp1, struct file_desc* filp2)
{
    int retval;

    if (filp1->fd_cnt > 0 && filp2->fd_cnt > 0) {
        unlock_inode(filp1->fd_inode);
    }

    retval = mutex_unlock(&filp1->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }

    retval = mutex_unlock(&filp2->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }
}

struct files_struct* files_alloc(void)
{
    struct files_struct* files;

    _Static_assert(BITCHUNKS(NR_OPEN_DEFAULT) == 1,
                   "NR_OPEN_DEFAULT > bitchunk bits");

    files = malloc(sizeof(*files));
    if (!files) return NULL;

    memset(files, 0, sizeof(*files));
    INIT_ATOMIC(&files->refcnt, 1);
    files->max_fds = NR_OPEN_DEFAULT;
    files->filp = files->filp_array;
    files->close_on_exec = &files->close_on_exec_init;

    return files;
}

static int expand_files(struct files_struct* files, unsigned int nr)
{
    int i;
    struct file_desc** new_filp;
    bitchunk_t* bitmap;
    size_t copy, set;

    nr /= (1024 / sizeof(struct file_desc*));
    for (i = 0; i < 5; i++) {
        nr |= nr >> (1 << i);
    }
    nr++;
    nr *= (1024 / sizeof(struct file_desc*));

    assert(nr > files->max_fds);

    new_filp = calloc(nr, sizeof(struct file_desc*));
    if (!new_filp) return ENOMEM;

    bitmap = calloc(BITCHUNKS(nr), sizeof(bitchunk_t));
    if (!bitmap) {
        free(new_filp);
        return ENOMEM;
    }

    copy = files->max_fds * sizeof(struct file_desc*);
    set = (nr - files->max_fds) * sizeof(struct file_desc*);

    assert(copy + set == nr * sizeof(struct file_desc*));
    memcpy(new_filp, files->filp, copy);
    memset((char*)new_filp + copy, 0, set);

    copy = BITCHUNKS(files->max_fds) * sizeof(bitchunk_t);
    set = BITCHUNKS(nr) * sizeof(bitchunk_t) - copy;

    assert(copy + set == BITCHUNKS(nr) * sizeof(bitchunk_t));
    memcpy(bitmap, files->close_on_exec, copy);
    memset((char*)bitmap + copy, 0, set);

    if (files->filp != files->filp_array) {
        free(files->filp);
        free(files->close_on_exec);
    }

    files->filp = new_filp;
    files->close_on_exec = bitmap;
    files->max_fds = nr;

    return 0;
}

struct files_struct* dup_files(struct files_struct* old, int* errorp)
{
    struct files_struct* new;
    int i, retval;

    *errorp = ENOMEM;

    new = malloc(sizeof(*new));
    if (!new) return NULL;

    memset(new, 0, sizeof(*new));
    INIT_ATOMIC(&new->refcnt, 1);
    new->max_fds = NR_OPEN_DEFAULT;
    new->filp = new->filp_array;
    new->close_on_exec = &new->close_on_exec_init;

    if (old->max_fds > new->max_fds) {
        retval = expand_files(new, old->max_fds);

        if (retval) {
            free(new);
            *errorp = retval;
            return NULL;
        }
    }

    assert(new->max_fds >= old->max_fds);

    for (i = 0; i < old->max_fds; i++) {
        struct file_desc* filp = old->filp[i];
        if (filp) {
            filp->fd_cnt++;
            new->filp[i] = filp;
        } else {
            new->filp[i] = NULL;
        }
    }

    memcpy(new->close_on_exec, old->close_on_exec,
           BITCHUNKS(old->max_fds) * sizeof(bitchunk_t));

    return new;
}

static void close_files(struct files_struct* files)
{
    int i;

    for (i = 0; i < files->max_fds; i++) {
        struct file_desc* filp = files->filp[i];

        if (filp) {
            lock_filp(filp, RWL_WRITE);
            close_filp(filp, FALSE);
        }
    }
}

void put_files(struct files_struct* files)
{
    if (atomic_dec_and_test(&files->refcnt)) {
        close_files(files);
        if (files->filp != files->filp_array) {
            free(files->close_on_exec);
            free(files->filp);
        }
        free(files);
    }
}

void exit_files(struct fproc* fp)
{
    struct files_struct* files = fp->files;

    fp->files = NULL;
    put_files(files);
}

int get_fd(struct fproc* fp, int start, mode_t bits, int* fd,
           struct file_desc** fpp)
{
    /* find an unused fd in proc's filp table and a free file slot */
    int i, retval;

    for (i = start; i < fp->files->max_fds; i++) {
        if (fp->files->filp[i] == 0) {
            *fd = i;
            break;
        }
    }
    if (i == fp->files->max_fds) {
        retval = expand_files(fp->files, i);
        if (retval) return retval;

        *fd = i;
    }

    if (GET_BIT(fp->files->close_on_exec, *fd)) {
        printl("%d %d\n", fp->endpoint, *fd);
    }
    assert(!GET_BIT(fp->files->close_on_exec, *fd));

    if (!fpp) return 0;

    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++) {
        struct file_desc* filp = &f_desc_table[i];

        if (filp->fd_inode == 0 && !mutex_trylock(&filp->fd_lock)) {
            filp->fd_mode = bits;
            INIT_LIST_HEAD(&filp->fd_ep_links);
            *fpp = filp;
            return 0;
        }
    }

    return ENFILE;
}

struct file_desc* get_filp(struct fproc* fp, int fd, rwlock_type_t lock_type)
{
    /* retrieve a file descriptor from fp's filp table and lock it */
    struct file_desc* filp = NULL;

    if (fd < 0 || fd >= fp->files->max_fds) {
        err_code = EINVAL;
        return NULL;
    }
    filp = fp->files->filp[fd];

    if (!filp) {
        err_code = EBADF;
    } else {
        lock_filp(filp, lock_type);
    }
    return filp;
}

void install_filp(struct fproc* fp, int fd, struct file_desc* filp)
{
    assert(fd < fp->files->max_fds);
    fp->files->filp[fd] = filp;
}

void set_close_on_exec(struct fproc* fp, int fd, int flag)
{
    assert(fd < fp->files->max_fds);
    if (flag)
        SET_BIT(fp->files->close_on_exec, fd);
    else
        UNSET_BIT(fp->files->close_on_exec, fd);
}

int close_filp(struct file_desc* filp, int may_block)
{
    struct inode* pin;

    assert(filp->fd_cnt > 0);
    pin = filp->fd_inode;

    if (--filp->fd_cnt == 0) {
        if (!list_empty(&filp->fd_ep_links)) eventpoll_release_file(filp);

        if (filp->fd_fops && filp->fd_fops->release) {
            filp->fd_fops->release(pin, filp, may_block);
        }

        unlock_inode(pin);
        put_inode(pin);
        filp->fd_inode = NULL;
    } else {
        unlock_inode(pin);
    }

    mutex_unlock(&filp->fd_lock);

    return 0;
}

int do_copyfd(void)
{
    endpoint_t endpoint = self->msg_in.u.m_vfs_copyfd.endpoint;
    int fd = self->msg_in.u.m_vfs_copyfd.fd;
    int how = self->msg_in.u.m_vfs_copyfd.how;
    int flags, retval;
    struct file_desc* filp;
    struct fproc* fp_dest;

    if ((fp_dest = vfs_endpt_proc(endpoint)) == NULL) return -EINVAL;

    flags = how & COPYFD_FLAGS_MASK;
    how &= ~COPYFD_FLAGS_MASK;

    filp = get_filp((how == COPYFD_TO) ? fproc : fp_dest, fd, RWL_READ);
    if (!filp) return -EBADF;

    switch (how) {
    case COPYFD_FROM:
        fp_dest = fproc;
        flags &= !COPYFD_CLOEXEC;
    /* fall-through */
    case COPYFD_TO:
        for (fd = 0; fd < fp_dest->files->max_fds; fd++)
            if (fp_dest->files->filp[fd] == NULL) break;

        if (fd == fp_dest->files->max_fds) {
            retval = expand_files(fp_dest->files, fd);
            if (retval) {
                retval = -retval;
                goto out;
            }
        }

        install_filp(fp_dest, fd, filp);
        filp->fd_cnt++;
        retval = fd;
        break;

    case COPYFD_CLOSE:
        if (filp->fd_cnt > 1) {
            filp->fd_cnt--;
            install_filp(fp_dest, fd, NULL);
            set_close_on_exec(fp_dest, fd, 0);
            retval = 0;
        } else
            retval = -EBADF;
        break;
    default:
        retval = -EINVAL;
    }

out:
    unlock_filp(filp);
    return retval;
}
