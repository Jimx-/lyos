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
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/driver.h>
#include <lyos/fs.h>
#include <lyos/sysutils.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"
#include "poll.h"

static struct vfs_mount* sockfs_vmnt;

static ssize_t sock_read(struct file_desc*, char*, size_t, loff_t*,
                         struct fproc*);
static ssize_t sock_write(struct file_desc*, const char*, size_t, loff_t*,
                          struct fproc*);
static int sock_release(struct inode* pin, struct file_desc* filp);

static const struct file_operations sock_fops = {
    .read = sock_read,
    .write = sock_write,
    .poll = sock_poll,
    .release = sock_release,
};

static ino_t get_next_ino(void)
{
    static ino_t last_ino = 1;
    return last_ino++;
}

void mount_sockfs(void)
{
    dev_t dev;

    if ((dev = get_none_dev()) == NO_DEV) {
        panic("vfs: cannot allocate dev for sockfs");
    }

    if ((sockfs_vmnt = get_free_vfs_mount()) == NULL) {
        panic("vfs: cannot allocate vfs mount for sockfs");
    }

    sockfs_vmnt->m_dev = dev;
    sockfs_vmnt->m_fs_ep = NO_TASK;
    sockfs_vmnt->m_flags = 0;
    strlcpy(sockfs_vmnt->m_label, "sockfs", FS_LABEL_MAX);

    sockfs_vmnt->m_mounted_on = NULL;
    sockfs_vmnt->m_root_node = NULL;
}

static int get_sock_inode(struct fproc* fp, dev_t dev, struct inode** ppin)
{
    ino_t ino = get_next_ino();

    struct inode* pin = new_inode(sockfs_vmnt->m_dev, ino,
                                  S_IFSOCK | S_IRWXU | S_IRWXG | S_IRWXO);

    if (!pin) {
        return ENOMEM;
    }

    pin->i_dev = sockfs_vmnt->m_dev;
    pin->i_num = ino;
    pin->i_gid = fproc->effgid;
    pin->i_uid = fproc->effuid;
    pin->i_size = 0;
    pin->i_fs_ep = NO_TASK;
    pin->i_specdev = dev;
    pin->i_vmnt = sockfs_vmnt;
    pin->i_fops = &sock_fops;
    pin->i_cnt++;

    *ppin = pin;

    return 0;
}

static int create_sock_fd(dev_t dev, int flags)
{
    int retval;
    int fd;
    struct inode* pin;
    struct file_desc* filp;

    if ((retval = lock_vmnt(sockfs_vmnt, RWL_READ)) != 0) return -retval;

    if ((retval = get_sock_inode(fproc, dev, &pin)) != 0) {
        unlock_vmnt(sockfs_vmnt);
        return -retval;
    }

    lock_inode(pin, RWL_READ);

    if ((retval = get_fd(fproc, 0, R_BIT | W_BIT, &fd, &filp)) != 0) {
        unlock_inode(pin);
        put_inode(pin);
        unlock_vmnt(sockfs_vmnt);
        return -retval;
    }

    fproc->filp[fd] = filp;
    filp->fd_cnt = 1;
    filp->fd_inode = pin;
    filp->fd_flags = O_RDWR | (flags & ~O_ACCMODE);

    filp->fd_fops = pin->i_fops;

    unlock_filp(filp);
    unlock_vmnt(sockfs_vmnt);

    return fd;
}

static int get_sock_flags(int type)
{
    int flags;

    flags = 0;
    if (type & SOCK_CLOEXEC) flags |= O_CLOEXEC;
    if (type & SOCK_NONBLOCK) flags |= O_NONBLOCK;

    return flags;
}

static int get_sock_fd(int fd, dev_t* dev, int* flags)
{
    struct file_desc* filp;

    filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return EBADF;

    if (!S_ISSOCK(filp->fd_inode->i_mode)) {
        unlock_filp(filp);
        return ENOTSOCK;
    }

    *dev = filp->fd_inode->i_specdev;
    if (flags) *flags = filp->fd_flags;

    unlock_filp(filp);
    return 0;
}

int do_socket(void)
{
    endpoint_t src = self->msg_in.source;
    int domain = self->msg_in.u.m_vfs_socket.domain;
    int type = self->msg_in.u.m_vfs_socket.type;
    int protocol = self->msg_in.u.m_vfs_socket.protocol;
    int sock_type, flags;
    dev_t dev;
    int retval;

    if ((retval = check_fds(fproc, 1)) != 0) return -retval;

    sock_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    flags = get_sock_flags(type);

    if ((retval = sdev_socket(src, domain, sock_type, protocol, &dev, FALSE)) !=
        0)
        return -retval;

    if ((retval = create_sock_fd(dev, flags)) < 0) {
        sdev_close(src, dev, FALSE);
    }

    return retval;
}

int do_socketpair(void)
{
    endpoint_t src = self->msg_in.source;
    int domain = self->msg_in.u.m_vfs_socket.domain;
    int type = self->msg_in.u.m_vfs_socket.type;
    int protocol = self->msg_in.u.m_vfs_socket.protocol;
    int sock_type, flags;
    dev_t dev[2];
    int fd0, fd1, retval;

    if ((retval = check_fds(fproc, 2)) != 0) return retval;

    sock_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    flags = get_sock_flags(type);

    if ((retval = sdev_socket(src, domain, sock_type, protocol, dev, TRUE)) !=
        0)
        return retval;

    if ((fd0 = create_sock_fd(dev[0], flags)) < 0) {
        sdev_close(src, dev[0], FALSE);
        sdev_close(src, dev[1], FALSE);
        return -fd0;
    }

    if ((fd1 = create_sock_fd(dev[1], flags)) < 0) {
        sdev_close(src, dev[0], FALSE);
        sdev_close(src, dev[1], FALSE);
        return -fd1;
    }

    self->msg_out.u.m_vfs_fdpair.fd0 = fd0;
    self->msg_out.u.m_vfs_fdpair.fd1 = fd1;

    return 0;
}

int do_bind(void)
{
    int fd = self->msg_in.u.m_vfs_bindconn.sock_fd;
    void* addr = self->msg_in.u.m_vfs_bindconn.addr;
    size_t addrlen = self->msg_in.u.m_vfs_bindconn.addr_len;
    dev_t dev;
    int flags, retval;

    if ((retval = get_sock_fd(fd, &dev, &flags)) != OK) return retval;

    return sdev_bind(fproc->endpoint, dev, addr, addrlen, flags);
}

int do_connect(void)
{
    int fd = self->msg_in.u.m_vfs_bindconn.sock_fd;
    void* addr = self->msg_in.u.m_vfs_bindconn.addr;
    size_t addrlen = self->msg_in.u.m_vfs_bindconn.addr_len;
    dev_t dev;
    int flags, retval;

    if ((retval = get_sock_fd(fd, &dev, &flags)) != OK) return retval;

    return sdev_connect(fproc->endpoint, dev, addr, addrlen, flags);
}

int do_listen(void)
{
    int fd = self->msg_in.u.m_vfs_listen.sock_fd;
    int backlog = self->msg_in.u.m_vfs_listen.backlog;
    dev_t dev;
    int retval;

    if (backlog < 0) backlog = 0;

    if ((retval = get_sock_fd(fd, &dev, NULL)) != OK) return retval;

    return sdev_listen(fproc->endpoint, dev, backlog);
}

int do_accept(void)
{
    int fd = self->msg_in.u.m_vfs_bindconn.sock_fd;
    void* addr = self->msg_in.u.m_vfs_bindconn.addr;
    size_t addrlen = self->msg_in.u.m_vfs_bindconn.addr_len;
    dev_t dev, newdev;
    int flags;
    int retval;

    if ((retval = get_sock_fd(fd, &dev, &flags)) != OK) return retval;

    if ((retval = check_fds(fproc, 1)) != OK) return retval;

    retval = sdev_accept(fproc->endpoint, dev, addr, &addrlen, flags, &newdev);

    if (retval != OK && newdev == NO_DEV) return -retval;

    if (retval != OK) {
        sdev_close(fproc->endpoint, newdev, FALSE);
        return -retval;
    }

    flags &= O_CLOEXEC | O_NONBLOCK;
    if ((retval = create_sock_fd(newdev, flags)) < 0)
        sdev_close(fproc->endpoint, newdev, FALSE);

    if (retval >= 0) self->msg_out.CNT = addrlen;

    return retval;
}

ssize_t do_sendto(void)
{
    int fd = self->msg_in.u.m_vfs_sendrecv.sock_fd;
    void* buf = self->msg_in.u.m_vfs_sendrecv.buf;
    size_t len = self->msg_in.u.m_vfs_sendrecv.len;
    int flags = self->msg_in.u.m_vfs_sendrecv.flags;
    void* addr = self->msg_in.u.m_vfs_sendrecv.addr;
    unsigned int addr_len = self->msg_in.u.m_vfs_sendrecv.addr_len;
    dev_t dev;
    int filp_flags;
    int retval;

    if ((retval = get_sock_fd(fd, &dev, &filp_flags)) != OK) return -retval;

    return sdev_readwrite(fproc->endpoint, dev, buf, len, addr, &addr_len,
                          flags, WRITE, filp_flags);
}

ssize_t do_recvfrom(void)
{
    int fd = self->msg_in.u.m_vfs_sendrecv.sock_fd;
    void* buf = self->msg_in.u.m_vfs_sendrecv.buf;
    size_t len = self->msg_in.u.m_vfs_sendrecv.len;
    int flags = self->msg_in.u.m_vfs_sendrecv.flags;
    void* addr = self->msg_in.u.m_vfs_sendrecv.addr;
    unsigned int addr_len = self->msg_in.u.m_vfs_sendrecv.addr_len;
    dev_t dev;
    int filp_flags;
    int retval;

    if ((retval = get_sock_fd(fd, &dev, &filp_flags)) != OK) return -retval;

    retval = sdev_readwrite(fproc->endpoint, dev, buf, len, addr, &addr_len,
                            flags, READ, filp_flags);

    if (retval >= 0) self->msg_out.u.m_vfs_sendrecv.addr_len = addr_len;

    return retval;
}

ssize_t do_sockmsg(void)
{
    endpoint_t src = fproc->endpoint;
    int fd = self->msg_in.u.m_vfs_sendrecv.sock_fd;
    void* buf = self->msg_in.u.m_vfs_sendrecv.buf;
    int flags = self->msg_in.u.m_vfs_sendrecv.flags;
    unsigned int ctl_len, addr_len;
    struct msghdr msg;
    struct iovec iov[NR_IOREQS];
    dev_t dev;
    int filp_flags;
    ssize_t retval;

    if ((retval = get_sock_fd(fd, &dev, &filp_flags)) != OK) return -retval;

    if ((retval = data_copy(SELF, &msg, src, buf, sizeof(msg))) != OK)
        return -retval;

    if (msg.msg_iovlen > 0) {
        if (msg.msg_iovlen > NR_IOREQS) return -EMSGSIZE;

        if ((retval = data_copy(SELF, iov, src, msg.msg_iov,
                                sizeof(iov[0]) * msg.msg_iovlen)) != OK)
            return -retval;
    }

    addr_len = msg.msg_namelen;
    ctl_len = msg.msg_controllen;

    retval = sdev_vreadwrite(src, dev, iov, msg.msg_iovlen, msg.msg_control,
                             &ctl_len, msg.msg_name, &addr_len, flags,
                             (self->msg_in.type == SENDMSG ? WRITE : READ),
                             filp_flags);

    if (retval >= 0) {
        msg.msg_controllen = ctl_len;
        if (addr_len) msg.msg_namelen = addr_len;

        if (data_copy(src, buf, SELF, &msg, sizeof(msg)) != OK) return -EFAULT;
    }

    return retval;
}

int do_getsockopt(void)
{
    endpoint_t src = self->msg_in.source;
    int fd = self->msg_in.u.m_vfs_sockopt.sock_fd;
    int level = self->msg_in.u.m_vfs_sockopt.level;
    int name = self->msg_in.u.m_vfs_sockopt.name;
    void* buf = self->msg_in.u.m_vfs_sockopt.buf;
    size_t len = self->msg_in.u.m_vfs_sockopt.len;
    dev_t dev;
    int retval;

    if ((retval = get_sock_fd(fd, &dev, NULL)) != OK) return retval;

    retval = sdev_getsockopt(src, dev, level, name, buf, &len);
    if (retval == OK) self->msg_out.u.m_vfs_sockopt.len = len;

    return retval;
}

int do_setsockopt(void)
{
    endpoint_t src = self->msg_in.source;
    int fd = self->msg_in.u.m_vfs_sockopt.sock_fd;
    int level = self->msg_in.u.m_vfs_sockopt.level;
    int name = self->msg_in.u.m_vfs_sockopt.name;
    void* buf = self->msg_in.u.m_vfs_sockopt.buf;
    size_t len = self->msg_in.u.m_vfs_sockopt.len;
    dev_t dev;
    int retval;

    if ((retval = get_sock_fd(fd, &dev, NULL)) != OK) return retval;

    return sdev_setsockopt(src, dev, level, name, buf, len);
}

static ssize_t sock_read(struct file_desc* filp, char* buf, size_t count,
                         loff_t* ppos, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;

    return sdev_readwrite(fp->endpoint, pin->i_specdev, buf, count, NULL, NULL,
                          0, READ, filp->fd_flags);
}

static ssize_t sock_write(struct file_desc* filp, const char* buf, size_t count,
                          loff_t* ppos, struct fproc* fp)
{
    struct inode* pin = filp->fd_inode;

    return sdev_readwrite(fp->endpoint, pin->i_specdev, (void*)buf, count, NULL,
                          NULL, 0, WRITE, filp->fd_flags);
}

static int sock_release(struct inode* pin, struct file_desc* filp)
{
    sdev_close(fproc->endpoint, pin->i_specdev, !(filp->fd_flags & O_NONBLOCK));
    return 0;
}
