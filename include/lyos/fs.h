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

#ifndef	_FS_H_
#define	_FS_H_

#include "lyos/list.h"
#include "lyos/spinlock.h"
   
#define MAJOR_NONE		0
#define NR_NONEDEVS		64 
/**
 * @struct dev_drv_map fs.h "include/sys/fs.h"
 * @brief  The Device_nr.\ - Driver_nr.\ MAP.
 */
struct dev_drv_map {
	int driver_nr; /**< The proc nr.\ of the device driver. */
};

extern  struct dev_drv_map  dd_map[];

#define	MAX_PATH	512
#define FS_LABEL_MAX 15

/* VFS/FS error messages */
#define EENTERMOUNT              (-301)
#define ELEAVEMOUNT              (-302)
#define ESYMLINK                 (-303)

/* Message types */
#define REQ_DEV            	u.m3.m3i1
#define REQ_NUM				u.m3.m3l2
#define REQ_START_INO       u.m3.m3i2
#define REQ_FILESIZE		u.m3.m3i2
#define REQ_ROOT_INO        u.m3.m3i3
#define REQ_NAMELEN         u.m3.m3i4
#define REQ_FLAGS           u.m3.m3l1
#define REQ_MODE			u.m3.m3l1
#define REQ_PATHNAME        u.m3.m3p1
#define REQ_STARTPOS		u.m3.m3i3
#define REQ_ENDPOS			u.m3.m3i4
#define REQ_UCRED			u.m3.m3p2

/* for stat */
#define STDEV		u.m3.m3i1
#define STINO		u.m3.m3i2
#define STBUF		u.m3.m3p1
#define STSRC 		u.m3.m3i3
#define STRET 		u.m3.m3i4

/* for rdwt */
#define RWDEV		u.m3.m3l2
#define RWINO		u.m3.m3i1
#define RWPOS		u.m3.m3l1
#define RWFLAG		u.m3.m3i2
#define RWSRC		u.m3.m3i3
#define RWBUF		u.m3.m3p1
#define RWCNT		u.m3.m3i4
#define RWRET		u.m3.m3l2

/* for create */
#define CRDEV            	u.m3.m3i1
#define CRRET 				u.m3.m3i1
#define CRINO				u.m3.m3i2
#define CRUID				u.m3.m3i3
#define CRGID				u.m3.m3i4
#define CRNAMELEN         	u.m3.m3l1
#define CRFILESIZE			u.m3.m3l1
#define CRMODE				u.m3.m3l2
#define CRPATHNAME        	u.m3.m3p1

/* for mm request */
#define MMRTYPE				u.m3.m3i1
#define MMRRESULT			u.m3.m3i1
#define MMR_FDLOOKUP		1
#define MMR_FDREAD			2
#define MMRFD				u.m3.m3i2
#define MMRENDPOINT			u.m3.m3i3
#define MMRDEV				u.m3.m3i4
#define MMRINO				u.m3.m3l1
#define MMROFFSET			u.m3.m3l1
#define MMRLENGTH			u.m3.m3l2
#define MMRBUF				u.m3.m3p1

#define RET_RETVAL          u.m5.m5i1
#define RET_NUM             u.m5.m5i2
#define RET_UID             u.m5.m5i3
#define RET_GID             u.m5.m5i4
#define RET_FILESIZE        u.m5.m5i5
#define RET_MODE            u.m5.m5i6
#define RET_OFFSET          u.m5.m5i7
#define RET_SPECDEV         u.m5.m5i8

#define MS_READONLY         0x001
#define RF_READONLY         0x001
#define RF_ISROOT           0x002

/**
 * @struct inode
 * @brief  i-node
 *
 * The \c start_sect and\c nr_sects locate the file in the device,
 * and the size show how many bytes is used.
 * If <tt> size < (nr_sects * SECTOR_SIZE) </tt>, the rest bytes
 * are wasted and reserved for later writing.
 *
 * \b NOTE: Remember to change INODE_SIZE if the members are changed
 */
struct inode {
	struct list_head list;
	endpoint_t	i_fs_ep;		/**< FS process's pid */
	u32	i_mode;		/**< Accsess mode */
	u32	i_size;		/**< File size */
	u32	i_start_sect;	/**< The first sector of the data */
	u32	i_nr_sects;	/**< How many sectors the file occupies */
	uid_t i_uid;  /* uid and gid */
	gid_t i_gid;
	dev_t i_dev;  /**< On which device this inode resides */
    dev_t i_specdev;  /**< Device number for block/character special file */
	int	i_cnt;		/**< How many procs share this inode  */
	int	i_num;		/**< inode nr.  */
	spinlock_t i_lock;

	struct vfs_mount * i_vmnt;
};

struct vfs_mount {
	struct list_head list;
  	int m_fs_ep;			/**< FS process's pid */
  	int m_dev;			/**< Device number */
  	unsigned int m_flags;		/**< Mount flags */
  	struct inode *m_mounted_on;	/**< Mount point */
  	struct inode *m_root_node;	/**< Root inode */
  	char m_label[FS_LABEL_MAX];	/**< Label of the file system process */
  	spinlock_t m_lock;
};

#define VMNT_READONLY       0x001

/**
 * @def   INODE_SIZE
 * @brief The size of i-node stored \b in \b the \b device.
 *
 * Note that this is the size of the struct in the device, \b NOT in memory.
 * The size in memory is larger because of some more members.
 */
#define	INODE_SIZE	32

/**
 * @def   MAX_FILENAME_LEN
 * @brief Max len of a filename
 * @see   dir_entry
 */
#define	MAX_FILENAME_LEN	12

/**
 * @struct file_desc
 * @brief  File Descriptor
 */
struct file_desc {
	int		fd_mode;	/**< R or W */
	int		fd_pos;		/**< Current position for R/W. */
	int		fd_cnt;		/**< How many procs share this desc */
	struct inode*	fd_inode;	/**< Ptr to the i-node */
};

struct file_system {
    struct list_head list;
	char name[FS_LABEL_MAX];
	int fs_ep;
};

PUBLIC void clear_vfs_mount(struct vfs_mount * vmnt);
PUBLIC struct vfs_mount * get_free_vfs_mount();
PUBLIC int do_vfs_open(MESSAGE * p);

/**
 * Since all invocations of `rw_sector()' in FS look similar (most of the
 * params are the same), we use this macro to make code more readable.
 */
#define RD_SECT(dev,sect_nr) rw_sector(DEV_READ, \
				       dev,				\
				       (sect_nr) * SECTOR_SIZE,		\
				       SECTOR_SIZE, /* read one sector */ \
				       TASK_FS,				\
				       fsbuf);
#define WR_SECT(dev,sect_nr) rw_sector(DEV_WRITE, \
				       dev,				\
				       (sect_nr) * SECTOR_SIZE,		\
				       SECTOR_SIZE, /* write one sector */ \
				       TASK_FS,				\
				       fsbuf);

	
/* User credential info */
struct vfs_ucred {
	uid_t uid;
	gid_t gid;
};

#define _POSIX_SYMLOOP_MAX	8

#endif /* _FS_H_ */
