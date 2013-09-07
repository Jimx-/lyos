#ifndef _EXT2_FS_H_
#define _EXT2_FS_H_

#include "buffer.h"

#define EXT2FS_MAGIC 0xEF53

#define EXT2_SUPERBLOCK_SIZE	1024
/* ext2 super block struct. */
struct ext2_superblock {
	u32 sb_inodes_count;
	u32 sb_blocks_count;
	u32 sb_r_blocks_count;
	u32 sb_free_blocks_count;
	u32 sb_free_inodes_count;
	u32 sb_first_data_block;
	u32 sb_log_block_size;
	u32 sb_log_frag_size;
	u32 sb_blocks_per_group;
	u32 sb_frags_per_group;
	u32 sb_inodes_per_group;
	u32 sb_mtime;
	u32 sb_wtime;

	u16 sb_mnt_count;
	u16 sb_max_mnt_count;
	u16 sb_magic;
	u16 sb_state;
	u16 sb_errors;
	u16 sb_minor_rev_level;

	u32 sb_lastcheck;
	u32 sb_checkinterval;
	u32 sb_creator_os;
	u32 sb_rev_level;

	u16 sb_def_resuid;
	u16 sb_def_resgid;

	/* EXT2_DYNAMIC_REV */
	u32 sb_first_ino;
	u16 sb_inode_size;
	u16 sb_block_group_nr;
	u32 sb_feature_compat;
	u32 sb_feature_incompat;
	u32 sb_feature_ro_compat;

	u8 sb_uuid[16];
	u8 sb_volume_name[16];

	u8 sb_last_mounted[64];

	u32 sb_algo_bitmap;

	/* Performance Hints */
	u8 sb_prealloc_blocks;
	u8 sb_prealloc_dir_blocks;
	u16 sb__padding;

	/* Journaling Support */
	u8 sb_journal_uuid[16];
	u32 sb_journal_inum;
	u32 sb_jounral_dev;
	u32 sb_last_orphan;

	/* Directory Indexing Support */
	u32 sb_hash_seed[4];
	u8 sb_def_hash_version;
	u16 sb__padding_a;
	u8 sb__padding_b;

	/* Other Options */
	u32 sb_default_mount_options;
	u32 sb_first_meta_bg;
	u8 sb__unused[760];

	/*------ 1024 bytes ------*/
	/* Only present in memory */
	struct list_head list;
	struct ext2_bgdescriptor * sb_bgdescs;

	dev_t sb_dev;
	u32 sb_block_size;
	u32 sb_groups_count;
	u32 sb_blocksize_bits;
	u8	sb_is_root;
	u8  sb_readonly;
    u8  sb_bgd_dirty;
} __attribute__ ((packed));

typedef struct ext2_superblock ext2_superblock_t;

#define EXT2_BLOCK_ADDRESS_BYTES	4

#define EXT2_NDIR_BLOCKS            12
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK         (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS           (EXT2_TIND_BLOCK + 1)

/* Block group descriptor. */
struct ext2_bgdescriptor {
	u32 block_bitmap;
	u32 inode_bitmap;
	u32 inode_table;
	u16 free_blocks_count;
	u16 free_inodes_count;
	u16 used_dirs_count;
	u16 pad;
	u8 reserved[12];
} __attribute__ ((packed));

typedef struct ext2_bgdescriptor ext2_bgdescriptor_t;

/* File Types */
#define EXT2_S_IFSOCK	0xC000
#define EXT2_S_IFLNK	0xA000
#define EXT2_S_IFREG	0x8000
#define EXT2_S_IFBLK	0x6000
#define EXT2_S_IFDIR	0x4000
#define EXT2_S_IFCHR	0x2000
#define EXT2_S_IFIFO	0x1000

/* setuid, etc. */
#define EXT2_S_ISUID	0x0800
#define EXT2_S_ISGID	0x0400
#define EXT2_S_ISVTX	0x0200

/* rights */
#define EXT2_S_IRUSR	0x0100
#define EXT2_S_IWUSR	0x0080
#define EXT2_S_IXUSR	0x0040
#define EXT2_S_IRGRP	0x0020
#define EXT2_S_IWGRP	0x0010
#define EXT2_S_IXGRP	0x0008
#define EXT2_S_IROTH	0x0004
#define EXT2_S_IWOTH	0x0002
#define EXT2_S_IXOTH	0x0001

#define EXT2_GOOD_OLD_INODE_SIZE	128
#define EXT2_GOOD_OLD_FIRST_INO		11

#define EXT2_GOOD_OLD_REV       0       /* The good old (original) format */
#define EXT2_DYNAMIC_REV        1       /* V2 format w/ dynamic inode sizes */

#define EXT2_INODE_SIZE(s)	(((s)->sb_rev_level == EXT2_GOOD_OLD_REV) ? \
				EXT2_GOOD_OLD_INODE_SIZE : \
							(s)->sb_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->sb_rev_level == EXT2_GOOD_OLD_REV) ? \
				EXT2_GOOD_OLD_FIRST_INO : \
							(s)->sb_first_ino)

#define EXT2_ROOT_INODE		((ino_t)2)
/* Inode. */
struct ext2_inode {
	u16 i_mode;
	u16 i_uid;
	u32 i_size;
	u32 i_atime;
	u32 i_ctime;
	u32 i_mtime;
	u32 i_dtime;
	u16 i_gid;
	u16 i_links_count;
	u32 i_blocks;
	u32 i_flags;
	union {
    	struct {
                u32  l_i_reserved1;
        } linux1;
        struct {
                u32  h_i_translator;
        } hurd1;
        struct {
                u32  m_i_reserved1;
        } masix1;
    } osd1;     
	u32 i_block[EXT2_N_BLOCKS];
	u32 i_generation;
	u32 i_file_acl;
	u32 i_dir_acl;
	u32 i_faddr;
	union {
        struct {
            u8    l_i_frag;       /* Fragment number */
            u8    l_i_fsize;      /* Fragment size */
            u16   i_pad1;
            u16  l_i_uid_high;   /* these 2 fields    */
            u16  l_i_gid_high;   /* were reserved2[0] */
            u32   l_i_reserved2;
        } linux2;
        struct {
            u8    h_i_frag;       /* Fragment number */
            u8    h_i_fsize;      /* Fragment size */
            u16  h_i_mode_high;
            u16  h_i_uid_high;
            u16  h_i_gid_high;
            u32  h_i_author;
        } hurd2;
        struct {
            u8    m_i_frag;       /* Fragment number */
            u8    m_i_fsize;      /* Fragment size */
            u16   m_pad1;
            u32   m_i_reserved2[2];
        } masix2;
    } osd2;                         /* OS dependent 2 */

    /*------- 128 bytes --------*/
    /* only present in memory */
    struct list_head 	list;
    ino_t 		i_num;
    dev_t 		i_dev;
    int 		i_count;
    ext2_superblock_t * i_sb;
    u8 			i_dirt;
    u8 			i_mountpoint;
    u32			i_update;
    off_t 		i_last_dpos;          /* where to start dentry search */
    int 		i_last_dentry_size;	/* size of last found dentry */
} __attribute__ ((packed));

#define INO_CLEAN 		0
#define INO_DIRTY 		1

typedef struct ext2_inode ext2_inode_t;

#define EXT2_NAME_LEN 255

#define SD_LOOK_UP            0 /* tells search_dir to lookup string */
#define SD_MAKE               1 /* tells search_dir to make dir entry */
#define SD_DELETE             2 /* tells search_dir to delete entry */
#define SD_IS_EMPTY           3 /* tells search_dir to check if directory is empty */

#define EXT2_MIN_DIR_ENTRY_SIZE	8
#define EXT2_DIR_ENTRY_ALIGN	4

#define EXT2_DIR_ENTRY_CONTENTS_SIZE(d) (EXT2_MIN_DIR_ENTRY_SIZE + (d)->d_name_len)

/* size with padding */
#define EXT2_DIR_ENTRY_ACTUAL_SIZE(d) (EXT2_DIR_ENTRY_CONTENTS_SIZE(d) + \
        ((EXT2_DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) == 0 ? 0 : \
			EXT2_DIR_ENTRY_ALIGN - (EXT2_DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) ))

/* How many bytes can be taken from the end of dentry */
#define EXT2_DIR_ENTRY_SHRINK(d)    ((d)->d_rec_len - EXT2_DIR_ENTRY_ACTUAL_SIZE(d))

/* Directory entry. */
struct ext2_dir_entry {
	u32 d_inode;
	u16 d_rec_len;
	u8 d_name_len;
	u8 d_file_type;
	char d_name[EXT2_NAME_LEN];
} __attribute__ ((packed));

typedef struct ext2_dir_entry ext2_dir_entry_t;

#define EXT2_COMPAT_DIR_PREALLOC        0x0001
#define EXT2_COMPAT_IMAGIC_INODES       0x0002
#define EXT2_COMPAT_HAS_JOURNAL         0x0004
#define EXT2_COMPAT_EXT_ATTR            0x0008
#define EXT2_COMPAT_RESIZE_INO          0x0010
#define EXT2_COMPAT_DIR_INDEX           0x0020
#define EXT2_COMPAT_ANY                 0xffffffff

#define EXT2_HAS_COMPAT_FEATURE(sb, mask)  ((sb)->sb_feature_compat & (mask))

#define EXT2_ERROR_FS           0x002

/* hash-indexed directory */
#define EXT2_INDEX_FL			0x00001000
/* Top of directory hierarchies*/
#define EXT2_TOPDIR_FL                  0x00020000

#define ATIME            002    /* set if atime field needs updating */
#define CTIME            004    /* set if ctime field needs updating */
#define MTIME            010    /* set if mtime field needs updating */

#define EXT2_BUFFER_WRITE_IMME  0x01    /* write the buffer immediately back to the disk */

PUBLIC int read_ext2_super_block(int dev);
PUBLIC int write_ext2_super_block(int dev);
PUBLIC ext2_superblock_t * get_ext2_super_block(int dev);
PUBLIC ext2_bgdescriptor_t * get_ext2_group_desc(ext2_superblock_t * psb, unsigned int desc_num);

PUBLIC void ext2_init_inode();
PUBLIC ext2_inode_t * get_ext2_inode(dev_t dev, ino_t num);
PUBLIC ext2_inode_t * find_ext2_inode(dev_t dev, ino_t num);
PUBLIC void put_ext2_inode(ext2_inode_t * pin);
PUBLIC int ext2_rw_inode(ext2_inode_t * inode, int rw_flag);

PUBLIC int ext2_forbidden(ext2_inode_t * pin, int access);

PUBLIC int ext2_lookup(MESSAGE * p);
PUBLIC int ext2_parse_path(dev_t dev, ino_t start, ino_t root, char * pathname, int flags, ext2_inode_t ** result, size_t * offsetp);
PUBLIC ext2_inode_t *ext2_advance(ext2_inode_t * dir_pin, char string[EXT2_NAME_LEN + 1],
								int check_perm);
PUBLIC int ext2_search_dir(ext2_inode_t * dir_pin, char string[EXT2_NAME_LEN + 1], ino_t *num, 
								int flag, int check_perm, int ftype);

PUBLIC block_t ext2_read_map(ext2_inode_t * pin, off_t position);

PUBLIC ext2_buffer_t * ext2_get_buffer(dev_t dev, block_t block);
PUBLIC void ext2_put_buffer(ext2_buffer_t * pb);
PUBLIC void ext2_sync_buffers();
PUBLIC int ext2_update_group_desc(ext2_superblock_t * psb, int desc);
PUBLIC void ext2_init_buffer_cache();
PUBLIC int ext2_readsuper(MESSAGE * p);

PUBLIC ext2_buffer_t * ext2_new_block(ext2_inode_t * pin, off_t position);
PUBLIC block_t ext2_alloc_block(ext2_inode_t * pin);
PUBLIC int ext2_setbit(bitchunk_t * bitmap, int max_bits, off_t startp);

PUBLIC int ext2_stat(MESSAGE * p);

#endif

