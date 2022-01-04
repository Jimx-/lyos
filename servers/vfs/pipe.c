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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

struct pipe_inode_info {
    struct wait_queue_head rd_wait, wr_wait;
    char* data;
    size_t start;
    size_t files, readers, writers;
};

static struct vfs_mount* pipefs_vmnt;

static int create_pipe(int* fds, int flags);

int do_pipe2(void)
{
    int flags;
    int fds[2];
    int retval;

    flags = self->msg_in.FLAGS;

    retval = create_pipe(fds, flags);

    if (!retval) {
        self->msg_out.u.m_vfs_fdpair.fd0 = fds[0];
        self->msg_out.u.m_vfs_fdpair.fd1 = fds[1];
    }

    return retval;
}

void mount_pipefs(void)
{
    dev_t dev;

    if ((dev = get_none_dev()) == NO_DEV) {
        panic("vfs: cannot allocate dev for pipefs");
    }

    if ((pipefs_vmnt = get_free_vfs_mount()) == NULL) {
        panic("vfs: cannot allocate vfs mount for pipefs");
    }

    pipefs_vmnt->m_dev = dev;
    pipefs_vmnt->m_fs_ep = NO_TASK;
    pipefs_vmnt->m_flags = 0;
    strlcpy(pipefs_vmnt->m_label, "pipefs", FS_LABEL_MAX);

    pipefs_vmnt->m_mounted_on = NULL;
    pipefs_vmnt->m_root_node = NULL;
}

static void pipe_wait(struct pipe_inode_info* pipe, int rw_flag)
{
    struct wait_queue_head* wq;
    DECLARE_WAITQUEUE(wait, self);

    if (rw_flag == READ) {
        wq = &pipe->rd_wait;
    } else {
        wq = &pipe->wr_wait;
    }

    waitqueue_add(wq, &wait);

    worker_wait(WT_BLOCKED_ON_PIPE);

    waitqueue_remove(wq, &wait);
}

static ssize_t pipe_read(struct file_desc* filp, char* buf, size_t count,
                         loff_t* ppos, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;
    struct pipe_inode_info* pipe = filp->fd_private_data;
    size_t size, cum_io;
    ssize_t retval;

    cum_io = 0;
    for (;;) {
        if (!pin->i_size) {
            if (pipe->writers) {
                waitqueue_wakeup_all(&pipe->wr_wait,
                                     (void*)(EPOLLOUT | EPOLLWRNORM));

                if (filp->fd_flags & O_NONBLOCK) {
                    return -EAGAIN;
                } else {
                    unlock_filp(filp);
                    pipe_wait(pipe, READ);
                    lock_filp(filp, RWL_READ);
                    continue;
                }
            }

            // no more data
            return 0;
        }

        size = count;

        if (size > pin->i_size) {
            size = pin->i_size;
        }

        retval =
            data_copy(fp->endpoint, buf, SELF, pipe->data + pipe->start, size);
        if (retval) {
            return -retval;
        }

        pipe->start += size;
        pin->i_size -= size;
        cum_io += size;

        break;
    }

    return cum_io;
}

static ssize_t pipe_write(struct file_desc* filp, const char* buf, size_t count,
                          loff_t* ppos, struct fproc* fp)
{
    loff_t pos;
    struct inode* pin = filp->fd_inode;
    struct pipe_inode_info* pipe = filp->fd_private_data;
    size_t size, cum_io;
    ssize_t retval;

    cum_io = 0;
    while (count > 0) {
        pos = pin->i_size;

        size = count;
        if (pos + size > PIPE_BUF) {
            if (filp->fd_flags & O_NONBLOCK) {
                if (size <= PIPE_BUF) {
                    return -EAGAIN;
                }

                size = PIPE_BUF - pos;

                if (size) {
                    // partial write, wake up readers
                    waitqueue_wakeup_all(&pipe->rd_wait,
                                         (void*)(EPOLLIN | EPOLLRDNORM));
                } else {
                    return -EAGAIN;
                }
            }

            if (size > PIPE_BUF) {
                size = PIPE_BUF - pos;

                if (size) {
                    // partial write, wake up readers
                    waitqueue_wakeup_all(&pipe->rd_wait,
                                         (void*)(EPOLLIN | EPOLLRDNORM));
                } else {
                    // pipe full
                    goto wait;
                }
            } else {
                // cannot do a partial write, wait for the readers
                goto wait;
            }
        }

        if (pos == 0) {
            waitqueue_wakeup_all(&pipe->rd_wait,
                                 (void*)(EPOLLIN | EPOLLRDNORM));
        }

        if (pipe->start > 0) {
            if (pin->i_size > 0) {
                assert(pin->i_size <= PIPE_BUF);
                memmove(pipe->data, pipe->data + pipe->start, pin->i_size);
            }

            pipe->start = 0;
        }

        assert(pin->i_size + size <= PIPE_BUF);
        retval = data_copy(SELF, pipe->data + pin->i_size, fp->endpoint,
                           (void*)buf, size);
        if (retval) {
            return -retval;
        }

        pin->i_size += size;
        buf += size;
        count -= size;
        cum_io += size;

        if (count > 0) {
            if (filp->fd_flags & O_NONBLOCK) {
                break;
            }
        } else {
            break;
        }

    wait:
        unlock_filp(filp);
        pipe_wait(pipe, WRITE);
        lock_filp(filp, RWL_WRITE);
    }

    return cum_io;
}

static __poll_t pipe_poll(struct file_desc* filp, __poll_t mask,
                          struct poll_table* wait, struct fproc* fp)
{
    struct pipe_inode_info* pipe = filp->fd_private_data;
    struct inode* pin = filp->fd_inode;

    if (filp->fd_mode & W_BIT) poll_wait(filp, &pipe->wr_wait, wait);
    if (filp->fd_mode & R_BIT) poll_wait(filp, &pipe->rd_wait, wait);

    mask = 0;

    if (filp->fd_mode & W_BIT) {
        if (pin->i_size < PIPE_BUF) {
            mask |= EPOLLOUT | EPOLLWRNORM;
        }

        if (!pipe->readers) {
            mask |= EPOLLERR;
        }
    }

    if (filp->fd_mode & R_BIT) {
        if (pin->i_size) {
            mask |= EPOLLIN | EPOLLRDNORM;
        }

        if (!pipe->writers) {
            mask |= EPOLLHUP;
        }
    }

    return mask;
}

static ino_t get_next_ino(void)
{
    static ino_t last_ino = 1;

    return last_ino++;
}

static struct pipe_inode_info* alloc_pipe_info(void)
{
    struct pipe_inode_info* pipe;

    pipe = malloc(sizeof(*pipe));
    if (!pipe) return NULL;

    memset(pipe, 0, sizeof(*pipe));

    pipe->data = malloc(PIPE_BUF);
    if (!pipe->data) {
        free(pipe);
        return NULL;
    }

    pipe->start = 0;
    init_waitqueue_head(&pipe->rd_wait);
    init_waitqueue_head(&pipe->wr_wait);

    return pipe;
}

static void free_pipe_info(struct pipe_inode_info* pipe)
{
    free(pipe->data);
    free(pipe);
}

static void put_pipe_info(struct inode* pin, struct pipe_inode_info* pipe)
{
    if (!--pipe->files) {
        pin->i_private = NULL;
        free_pipe_info(pipe);
    }
}

static int pipe_open(int fd, struct inode* pin, struct file_desc* filp)
{
    struct pipe_inode_info* pipe = pin->i_private;

    pipe->files++;

    if (filp->fd_mode & W_BIT) pipe->writers++;
    if (filp->fd_mode & R_BIT) pipe->readers++;

    return 0;
}

static int pipe_release(struct inode* pin, struct file_desc* filp)
{
    struct pipe_inode_info* pipe = filp->fd_private_data;

    if (filp->fd_mode & W_BIT) pipe->writers--;
    if (filp->fd_mode & R_BIT) pipe->readers--;

    if (!pipe->readers != !pipe->writers) {
        // wake up the other end
        if (!pipe->writers) {
            waitqueue_wakeup_all(&pipe->rd_wait, (void*)EPOLLHUP);
        }

        if (!pipe->readers) {
            waitqueue_wakeup_all(&pipe->wr_wait, (void*)EPOLLERR);
        }
    }

    put_pipe_info(pin, pipe);

    return 0;
}

static struct file_operations pipe_fops = {
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
    .open = pipe_open,
    .release = pipe_release,
};

static struct inode* get_pipe_inode(struct fproc* fp)
{
    ino_t ino = get_next_ino();

    struct inode* pin =
        new_inode(pipefs_vmnt->m_dev, ino, S_IFIFO | S_IRUSR | S_IWUSR);

    if (!pin) {
        err_code = ENOMEM;
        return NULL;
    }

    struct pipe_inode_info* pipe = alloc_pipe_info();
    if (!pipe) {
        put_inode(pin);
        err_code = ENOMEM;
        return NULL;
    }

    pipe->files = 2;
    pipe->readers = pipe->writers = 1;

    pin->i_dev = pipefs_vmnt->m_dev;
    pin->i_num = ino;
    pin->i_gid = fproc->realgid;
    pin->i_uid = fproc->realuid;
    pin->i_size = 0;
    pin->i_fs_ep = NO_TASK;
    pin->i_specdev = NO_DEV;
    pin->i_vmnt = pipefs_vmnt;
    pin->i_fops = &pipe_fops;
    pin->i_private = pipe;

    dup_inode(pin);
    pin->i_fs_cnt++;

    return pin;
}

static int create_pipe(int* fds, int flags)
{
    int retval;
    struct inode* pin;
    struct file_desc *filp0, *filp1;

    retval = lock_vmnt(pipefs_vmnt, RWL_READ);
    if (retval) return retval;

    pin = get_pipe_inode(fproc);
    if (!pin) {
        return err_code;
    }

    lock_inode(pin, RWL_READ);

    if ((retval = get_fd(fproc, 0, R_BIT, &fds[0], &filp0)) != 0) {
        unlock_inode(pin);
        put_inode(pin);
        unlock_vmnt(pipefs_vmnt);
        return retval;
    }
    install_filp(fproc, fds[0], filp0);
    filp0->fd_cnt = 1;

    if ((retval = get_fd(fproc, 0, W_BIT, &fds[1], &filp1)) != 0) {
        install_filp(fproc, fds[0], NULL);
        filp0->fd_cnt = 0;
        unlock_filp(filp0);
        unlock_inode(pin);
        put_inode(pin);
        unlock_vmnt(pipefs_vmnt);
        return retval;
    }
    install_filp(fproc, fds[1], filp1);
    filp1->fd_cnt = 1;

    filp0->fd_inode = pin;
    dup_inode(pin);
    filp1->fd_inode = pin;
    filp0->fd_flags = O_RDONLY | (flags & ~O_ACCMODE);
    filp1->fd_flags = O_WRONLY | (flags & ~O_ACCMODE);

    filp0->fd_fops = filp1->fd_fops = pin->i_fops;
    filp0->fd_private_data = filp1->fd_private_data = pin->i_private;

    set_close_on_exec(fproc, fds[0], flags & O_CLOEXEC);
    set_close_on_exec(fproc, fds[1], flags & O_CLOEXEC);

    unlock_filps(filp0, filp1);
    unlock_vmnt(pipefs_vmnt);

    return 0;
}
