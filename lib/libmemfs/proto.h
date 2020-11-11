#ifndef _LIBMEMFS_PROTO_H_
#define _LIBMEMFS_PROTO_H_

int memfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                    struct fsdriver_node* node);
int memfs_mountpoint(dev_t dev, ino_t num);
int memfs_lookup(dev_t dev, ino_t start, const char* name,
                 struct fsdriver_node* fn, int* is_mountpoint);
int memfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data);
ssize_t memfs_read(dev_t dev, ino_t num, struct fsdriver_data* data,
                   loff_t rwpos, size_t count);
ssize_t memfs_write(dev_t dev, ino_t num, struct fsdriver_data* data,
                    loff_t rwpos, size_t count);
ssize_t memfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                       loff_t* ppos, size_t count);
ssize_t memfs_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                     size_t bytes, endpoint_t user_endpt);
int memfs_symlink(dev_t dev, ino_t dir_num, const char* name, uid_t uid,
                  gid_t gid, struct fsdriver_data* data, size_t bytes);
int memfs_mkdir(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid);
int memfs_mknod(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid, dev_t sdev);
int memfs_chmod(dev_t dev, ino_t num, mode_t* mode);
int memfs_chown(dev_t dev, ino_t num, uid_t uid, gid_t gid, mode_t* mode);

void memfs_init_inode();
struct memfs_inode* memfs_new_inode(ino_t num, const char* name, int index);
struct memfs_inode* memfs_find_inode(ino_t num);
void memfs_addhash_inode(struct memfs_inode* inode);

int memfs_init_buf(void);
int memfs_free_buf(void);

#endif
