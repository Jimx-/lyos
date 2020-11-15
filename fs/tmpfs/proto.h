#ifndef _TMPFS_PROTO_H_
#define _TMPFS_PROTO_H_

/* super.c */
struct tmpfs_superblock* tmpfs_get_superblock(dev_t dev);
int tmpfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                    struct fsdriver_node* node);

/* inode.c */
void init_inode(void);
struct tmpfs_inode* tmpfs_alloc_inode(struct tmpfs_superblock* sb, ino_t num);
void tmpfs_dup_inode(struct tmpfs_inode* pin);
struct tmpfs_inode* tmpfs_get_inode(dev_t dev, ino_t num);
int tmpfs_put_inode(struct tmpfs_inode* pin);
int tmpfs_putinode(dev_t dev, ino_t num, unsigned int count);
int tmpfs_new_inode(struct tmpfs_inode* dir_pin, const char* pathname,
                    mode_t mode, uid_t uid, gid_t gid,
                    struct tmpfs_inode** ppin);
int tmpfs_inode_getpage(struct tmpfs_inode* pin, unsigned int index,
                        char** page);

/* stat.c */
int tmpfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data);

/* lookup.c */
int tmpfs_lookup(dev_t dev, ino_t start, const char* name,
                 struct fsdriver_node* fn, int* is_mountpoint);
int tmpfs_advance(struct tmpfs_inode* parent, const char* name,
                  struct tmpfs_inode** ppin);
int tmpfs_search_dir(struct tmpfs_inode* dir_pin, const char* string,
                     struct tmpfs_inode** ppin, int flag);

/* read_write.c */
ssize_t tmpfs_read(dev_t dev, ino_t num, struct fsdriver_data* data,
                   loff_t rwpos, size_t count);
ssize_t tmpfs_write(dev_t dev, ino_t num, struct fsdriver_data* data,
                    loff_t rwpos, size_t count);
ssize_t tmpfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                       loff_t* ppos, size_t count);

/* open.c */
int tmpfs_create(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                 uid_t uid, gid_t gid, struct fsdriver_node* fn);
int tmpfs_mkdir(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid);
int tmpfs_mknod(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid, dev_t sdev);

/* link.c */
int tmpfs_link(dev_t dev, ino_t dir_num, const char* name, ino_t num);
int tmpfs_unlink(dev_t dev, ino_t dir_num, const char* name);
int tmpfs_rmdir(dev_t dev, ino_t dir_num, const char* name);
ssize_t tmpfs_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                     size_t bytes, endpoint_t user_endpt);
int tmpfs_symlink(dev_t dev, ino_t dir_num, const char* name, uid_t uid,
                  gid_t gid, struct fsdriver_data* data, size_t bytes);

/* protect.c */
int tmpfs_chmod(dev_t dev, ino_t num, mode_t* mode);

/* time.c */
int tmpfs_utime(dev_t dev, ino_t num, const struct timespec* atime,
                const struct timespec* mtime);

#endif
