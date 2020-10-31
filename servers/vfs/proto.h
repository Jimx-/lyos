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

#ifndef _VFS_PROTO_H_
#define _VFS_PROTO_H_

#include <lyos/types.h>
#include <sys/uio.h>

#include "path.h"
#include "thread.h"

int vfs_verify_endpt(endpoint_t ep, int* proc_nr);
struct fproc* vfs_endpt_proc(endpoint_t ep);

int do_register_filesystem();
int add_filesystem(endpoint_t fs_ep, char* name);
endpoint_t get_filesystem_endpoint(char* name);

void init_inode_table();
struct inode* new_inode(dev_t dev, ino_t num, mode_t mode);
struct inode* find_inode(dev_t dev, ino_t num);
void put_inode(struct inode* pin);
int lock_inode(struct inode* pin, rwlock_type_t type);
void unlock_inode(struct inode* pin);
void sync_inode(struct inode* p);

void init_lookup(struct lookup* lookup, char* pathname, int flags,
                 struct vfs_mount** vmnt, struct inode** inode);
void init_lookupat(struct lookup* lookup, int dfd, char* pathname, int flags,
                   struct vfs_mount** vmnt, struct inode** inode);
struct inode* resolve_path(struct lookup* lookup, struct fproc* fp);
struct inode* advance_path(struct inode* start, struct lookup* lookup,
                           struct fproc* fp);
struct inode* last_dir(struct lookup* lookup, struct fproc* fp);
int do_socketpath(void);

struct vfs_mount* find_vfs_mount(dev_t dev);
int lock_vmnt(struct vfs_mount* vmnt, rwlock_type_t type);
void unlock_vmnt(struct vfs_mount* vmnt);
dev_t get_none_dev(void);
int mount_fs(dev_t dev, char* mountpoint, endpoint_t fs_ep, int readonly);
int forbidden(struct fproc* fp, struct inode* pin, int access);
mode_t do_umask(void);
void clear_vfs_mount(struct vfs_mount* vmnt);
struct vfs_mount* get_free_vfs_mount();
int do_vfs_open(MESSAGE* p);

int request_put_inode(endpoint_t fs_e, dev_t dev, ino_t num);
int request_lookup(endpoint_t fs_e, dev_t dev, ino_t start, ino_t root,
                   struct lookup* lookup, struct fproc* fp,
                   struct lookup_result* ret);
int request_readsuper(endpoint_t fs_ep, dev_t dev, int readonly, int is_root,
                      struct lookup_result* res);

int do_open(void);
int do_openat(void);
int common_openat(int dfd, char* pathname, int flags, mode_t mode);
int do_close(void);
int close_fd(struct fproc* fp, int fd);
int do_lseek(void);
int request_mknod(endpoint_t fs_ep, dev_t dev, ino_t num, uid_t uid, gid_t gid,
                  char* last_component, mode_t mode, dev_t sdev);

int do_chroot(MESSAGE* p);
int do_mount(void);
int do_umount(MESSAGE* p);
int do_mkdir(void);
int do_mknod(void);

/* vfs/read_write.c */
int do_rdwt(void);
int do_getdents(void);

/* vfs/link.c */
int do_unlink(void);
int do_unlinkat(void);
int do_rdlink(void);
int do_rdlinkat(void);
int do_symlink(void);
int truncate_node(struct inode* pin, int newsize);

int do_dup(void);
int do_chdir(void);
int do_fchdir(void);

int do_mm_request(void);
int fs_exec(void);

int request_stat(endpoint_t fs_ep, dev_t dev, ino_t num, endpoint_t src,
                 char* buf);

int request_readwrite(endpoint_t fs_ep, dev_t dev, ino_t num, loff_t pos,
                      int rw_flag, endpoint_t src, const void* buf,
                      size_t nbytes, loff_t* newpos, size_t* bytes_rdwt);

int do_stat(void);
int do_lstat(void);
int do_fstat(void);
int do_fstatat(void);
int do_access(void);
int do_chmod(int type);
int fs_getsetid(void);

/* vfs/device.c */
int do_ioctl(void);
mgrant_id_t make_ioctl_grant(endpoint_t driver_ep, endpoint_t user_ep,
                             int request, void* buf);

int do_fcntl(void);

int do_sync(void);

int fs_fork(void);
int fs_exit(void);

/* vfs/worker.c */
int rwlock_lock(rwlock_t* rwlock, rwlock_type_t lock_type);
void worker_init(void);
void worker_yield(void);
void worker_wait(int why);
void worker_wake(struct worker_thread* worker);
void worker_dispatch(struct fproc* fp, void (*func)(void), MESSAGE* msg);
void worker_allow(int allow);
struct worker_thread* worker_suspend(int why);
void worker_resume(struct worker_thread* worker);
void revive_proc(endpoint_t endpoint, MESSAGE* msg);
struct worker_thread* worker_get(thread_t tid);

/* vfs/ipc.c */
int fs_sendrec(endpoint_t fs_e, MESSAGE* msg);

/* vfs/file.c */
void lock_filp(struct file_desc* filp, rwlock_type_t lock_type);
void unlock_filp(struct file_desc* filp);
void unlock_filps(struct file_desc* filp1, struct file_desc* filp2);
struct file_desc* get_filp(struct fproc* fp, int fd, rwlock_type_t lock_type);
int check_fds(struct fproc* fp, int nfds);
int get_fd(struct fproc* fp, int start, mode_t bits, int* fd,
           struct file_desc** fpp);
int do_copyfd(void);

/* vfs/cdev.c */
void init_cdev(void);
int cdev_mmap(dev_t dev, endpoint_t src, void* vaddr, off_t offset,
              size_t length, void** retaddr, struct fproc* fp);
int cdev_reply(MESSAGE* msg);

/* vfs/select.c */
void init_select(void);
int do_select(void);
int do_poll(void);

/* vfs/pipe.c*/
int do_pipe2(void);
void mount_pipefs(void);

/* vfs/eventfd.c */
int do_eventfd(void);

/* vfs/anon_inodes.c */
void mount_anonfs(void);
int anon_inode_get_fd(struct fproc* fproc, int start,
                      const struct file_operations* fops, void* private,
                      int flags);

/* vfs/signalfd.c */
int do_signalfd(void);
int do_pm_signalfd_reply(MESSAGE* msg);

/* vfs/timerfd.c */
int do_timerfd_create(void);
int do_timerfd_settime(void);
int do_timerfd_gettime(void);

/* vfs/eventpoll.c */
int do_epoll_create1(void);
int do_epoll_ctl(void);
int do_epoll_wait(void);
void eventpoll_release_file(struct file_desc* filp);

/* vfs/driver.c */
int do_mapdriver(void);

/* vfs/sdev.c */
void init_sdev(void);
int sdev_mapdriver(const char* label, endpoint_t endpoint, const int* domains,
                   int nr_domains);
int sdev_socket(endpoint_t src, int domain, int type, int protocol, dev_t* dev,
                int pair);
int sdev_bind(endpoint_t src, dev_t dev, void* addr, size_t addrlen, int flags);
int sdev_connect(endpoint_t src, dev_t dev, void* addr, size_t addrlen,
                 int flags);
int sdev_listen(endpoint_t src, dev_t dev, int backlog);
int sdev_accept(endpoint_t src, dev_t dev, void* addr, size_t* addrlen,
                int flags, dev_t* newdev);
ssize_t sdev_readwrite(endpoint_t src, dev_t dev, void* data_buf,
                       size_t data_len, void* addr_buf, unsigned int* addr_len,
                       int flags, int rw_flag, int filp_flags);
ssize_t sdev_vreadwrite(endpoint_t src, dev_t dev, const struct iovec* iov,
                        size_t iov_len, void* ctl_buf, unsigned int* ctl_len,
                        void* addr_buf, unsigned int* addr_len, int flags,
                        int rw_flag, int filp_flags);
int sdev_getsockopt(endpoint_t src, dev_t dev, int level, int name, void* addr,
                    size_t* len);
int sdev_setsockopt(endpoint_t src, dev_t dev, int level, int name, void* addr,
                    size_t len);
int sdev_close(endpoint_t src, dev_t dev, int may_block);
__poll_t sock_poll(struct file_desc* filp, __poll_t mask,
                   struct poll_table* wait, struct fproc* fp);
void sdev_reply(MESSAGE* msg);

/* vfs/socket.c */
void mount_sockfs(void);
int do_socket(void);
int do_socketpair(void);
int do_bind(void);
int do_connect(void);
int do_listen(void);
int do_accept(void);
ssize_t do_sendto(void);
ssize_t do_recvfrom(void);
ssize_t do_sockmsg(void);
int do_getsockopt(void);
int do_setsockopt(void);

/* vfs/lock.c */
int do_lock(int fd, int req, void* arg);

#endif
