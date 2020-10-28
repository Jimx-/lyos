#ifndef _LIBMEMFS_PROTO_H_
#define _LIBMEMFS_PROTO_H_

int memfs_readsuper(dev_t dev, int flags, struct fsdriver_node* node);
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
int memfs_mkdir(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid);
int memfs_mknod(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid, dev_t sdev);

void memfs_init_inode();
struct memfs_inode* memfs_new_inode(ino_t num, const char* name, int index);
void memfs_set_inode_stat(struct memfs_inode* pin, struct memfs_stat* stat);
struct memfs_inode* memfs_find_inode(ino_t num);
void memfs_addhash_inode(struct memfs_inode* inode);

int memfs_init_buf(void);
int memfs_free_buf(void);

#endif
