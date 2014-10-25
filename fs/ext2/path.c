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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

PRIVATE char dot2[2] = "..";

PUBLIC int ext2_lookup(MESSAGE * p)
{
	int src = p->source;
	int dev = p->REQ_DEV;
	int start = p->REQ_START_INO;
	int root = p->REQ_ROOT_INO;
	int flags = (int)p->REQ_FLAGS;
	int name_len = p->REQ_NAMELEN;

    size_t offsetp = 0;
	ext2_inode_t * pin = NULL;

	char pathname[MAX_PATH];

	data_copy(SELF, pathname, src, p->REQ_PATHNAME, name_len);
	pathname[name_len] = '\0';

	int retval = ext2_parse_path(dev, start, root, pathname, flags, &pin, &offsetp);

	/* report error and offset position */
	if (retval == ELEAVEMOUNT || retval == EENTERMOUNT) {
		p->RET_OFFSET = offsetp;
		if (retval == EENTERMOUNT) put_ext2_inode(pin);
		return retval;
	}

	if (retval) return retval;

	/* fill result */
	p->RET_NUM = pin->i_num;
	p->RET_UID = pin->i_uid;
	p->RET_GID = pin->i_gid;
	p->RET_FILESIZE = pin->i_size;
	p->RET_MODE = pin->i_mode;
    p->RET_SPECDEV = pin->i_block[0];

	return 0;
}

PRIVATE char * ext2_get_name(char * pathname, char string[EXT2_NAME_LEN + 1]);

PUBLIC int ext2_parse_path(dev_t dev, ino_t start, ino_t root, char * pathname, int flags, ext2_inode_t ** result, size_t * offsetp)
{
	ext2_inode_t * pin, * dir_pin;
	char * cp, * next;
	char component[EXT2_NAME_LEN + 1];
	int leaving_mount;

	cp = pathname;

	if (!(pin = find_ext2_inode(dev, start))) return ENOENT;

	if (pin->i_links_count == 0) return ENOENT;

	leaving_mount = pin->i_mountpoint;
	++pin->i_count;
	/* scan */
	while (1) {
		if (cp[0] == '\0') {
			*result = pin;
			*offsetp = cp - pathname;

			if (pin->i_mountpoint) {
				return EENTERMOUNT;
			}

			return 0;
		}

		while (cp[0] == '/') cp++;

		if (!(next = ext2_get_name(cp, component))) {
			put_ext2_inode(pin);
			return err_code;
		}

		if (strcmp(component, "..") == 0) {
			int r;
			if ((r = ext2_forbidden(pin, X_BIT)) != 0) {
				put_ext2_inode(pin);
				return r;
			}

			/* ignore */
			if (pin->i_num == root) {
				cp = next;
				continue;
			}

			/* climb up to top fs */
			if (pin->i_num == EXT2_ROOT_INODE && !pin->i_sb->sb_is_root) {
				put_ext2_inode(pin);
				*offsetp = cp - pathname;
				return ELEAVEMOUNT;
			}
		}

		/* enter a child fs */
		if (!leaving_mount && pin->i_mountpoint) {
			*result = pin;
			*offsetp = cp - pathname;
			return EENTERMOUNT;
		}

		dir_pin = pin;
		pin = ext2_advance(dir_pin, leaving_mount ? dot2 : component, 1);
		if ((err_code == EENTERMOUNT) || (err_code == ELEAVEMOUNT)) err_code = 0;

		if (err_code) {
			put_ext2_inode(dir_pin);
			return err_code;
		}

		leaving_mount = 0;

		/* next component */
		put_ext2_inode(dir_pin);
		cp = next;
	}
}

/* find component in dir_pin */
PUBLIC ext2_inode_t *ext2_advance(ext2_inode_t * dir_pin, char string[EXT2_NAME_LEN + 1],
								int check_perm)
{
	ino_t num;
	ext2_inode_t * pin;

  	/* If 'string' is empty, return an error. */
  	if (string[0] == '\0') {
		err_code = ENOENT;
		return NULL;
  	}

  	/* Check for NULL. */
  	if (!dir_pin) return NULL;

  	/* If 'string' is not present in the directory, return error. */
 	if ((err_code = ext2_search_dir(dir_pin, string, &num, SD_LOOK_UP,
			      check_perm, 0)) != 0) {
		return NULL;
  	}

  	/* The component has been found in the directory.  Get inode. */
  	if ((pin = get_ext2_inode(dir_pin->i_dev, (int)num)) == NULL)  {
		return NULL;
  	}

  	if (pin->i_num == EXT2_ROOT_INODE) {
	  	if (dir_pin->i_num == EXT2_ROOT_INODE) {
		  	if (string[1] == '.') {
			  	if (!pin->i_sb->sb_is_root) {
				  	/* Climbing up mountpoint */
				  	err_code = ELEAVEMOUNT;
			  	}
		  	}
	  	}
  	}

  	if (pin->i_mountpoint) {
	  	/* Mountpoint encountered, report it */
	  	err_code = EENTERMOUNT;
  	}

  	return pin;
}

PRIVATE char * ext2_get_name(char * pathname, char string[EXT2_NAME_LEN + 1])
{
	size_t len;
	char * cp, * end;

	cp = pathname;
	end = cp;

	while(end[0] != '\0' && end[0] != '/') end++;

	len = (size_t)(end - cp);

	if (len > EXT2_NAME_LEN) {
		err_code = ENAMETOOLONG;
		return NULL;
	}

	if (len == 0) {
		strcpy(string, ".");
	} else {
		memcpy(string, cp, len);
		string[len] = '\0';
	}

	return end;
}

/* search file named string in dir_pin */
/* if flag == SD_MAKE, create it
 * if flag == SD_DELETE, delete it
 * if flag == SD_LOOK_UP, look it up and return it inode num 
 * if flag == SD_IS_EMPTY, check whether this directory is empty */
PUBLIC int ext2_search_dir(ext2_inode_t * dir_pin, char string[EXT2_NAME_LEN + 1], ino_t *num, 
								int flag, int check_perm, int ftype)
{
	ext2_dir_entry_t * pde, * prev_pde;
	off_t pos;
	block_t b;
	int hit = 0;
	int match = 0;
	unsigned new_slots = 0;
	int  required_space = 0;
	if ((dir_pin->i_mode & I_TYPE) != I_DIRECTORY) return ENOTDIR;
	int ret = 0;
	if (flag != SD_IS_EMPTY) {
		mode_t bits = (flag == SD_LOOK_UP ? X_BIT : W_BIT | X_BIT);
		if ((strcmp(string, ".") == 0) || (strcmp(string, "..") == 0)) {
			if (flag != SD_LOOK_UP) ret = dir_pin->i_sb->sb_readonly;
			else if (check_perm) ret = ext2_forbidden(dir_pin, bits);
		}
	}

	pos = 0;
	if (ret != 0) return ret;
	
	/* calculate the required space */
	if (flag == SD_MAKE) {
		int len = strlen(string);
		required_space = EXT2_MIN_DIR_ENTRY_SIZE + len;
		required_space += (required_space & 0x03) == 0 ? 0 :
			     (EXT2_DIR_ENTRY_ALIGN - (required_space & 0x03));
		if (dir_pin->i_last_dpos < dir_pin->i_size &&
				dir_pin->i_last_dentry_size <= required_space)
			pos = dir_pin->i_last_dpos;
	}

    ext2_buffer_t * pb = NULL;
	for (; pos < dir_pin->i_size; pos += dir_pin->i_sb->sb_block_size) {
		b = ext2_read_map(dir_pin, pos);

        if ((pb = ext2_get_buffer(dir_pin->i_dev, b)) == NULL) return err_code;
		prev_pde = NULL;

		for (pde = (ext2_dir_entry_t*)pb->b_data;
				((char *)pde - (char*)pb->b_data) < dir_pin->i_sb->sb_block_size;
				pde = (ext2_dir_entry_t*)((char*)pde + pde->d_rec_len) ) {

			match = 0;
			if (flag != SD_MAKE && pde->d_inode != (ino_t)0) {
				if (flag == SD_IS_EMPTY) {
					if (memcmp(pde->d_name, ".", 1) != 0 &&
					    memcmp(pde->d_name, "..", 2) != 0) match = 1; /* dir is not empty */
				} else {
					if (memcmp(pde->d_name, string, pde->d_name_len) == 0 && pde->d_name_len == strlen(string)){
						match = 1;
					}
				}
			}

			ret = 0;
			if (match) {
				if (flag == SD_IS_EMPTY) ret = ENOTEMPTY;	/* not empty */
				else if (flag == SD_DELETE) {
					if (pde->d_name_len >= sizeof(ino_t)) {
						/* Save inode number for recovery. */
						int ino_offset = pde->d_name_len - sizeof(ino_t);
						*((ino_t *) &pde->d_name[ino_offset]) = pde->d_inode;
					}
					pde->d_inode = (ino_t)0;	/* erase entry */
					if (!EXT2_HAS_COMPAT_FEATURE(dir_pin->i_sb,
								EXT2_COMPAT_DIR_INDEX))
						dir_pin->i_flags &= ~EXT2_INDEX_FL;
					if (pos < dir_pin->i_last_dpos) {
						dir_pin->i_last_dpos = pos;
						dir_pin->i_last_dentry_size = pde->d_rec_len;
					}
					dir_pin->i_update |= CTIME | MTIME;
					dir_pin->i_dirt = INO_DIRTY;

					if (prev_pde) {
						prev_pde->d_rec_len += pde->d_rec_len;
					}
				} else {
					*num = (ino_t)pde->d_inode;
				}
                ext2_put_buffer(pb);
				return ret;
			}

			/* Check for free slot */
			if (flag == SD_MAKE && pde->d_inode == 0) {
				/* we found a free slot, check if it has enough space */
				if (required_space <= pde->d_rec_len) {
					hit = 1;	/* we found a free slot */
					break;
				}
			}

			/* Can we shrink dentry? */
			if (flag == SD_MAKE && required_space <= EXT2_DIR_ENTRY_SHRINK(pde)) {
				int actual_size = EXT2_DIR_ENTRY_ACTUAL_SIZE(pde);
				int new_slot_size = pde->d_rec_len;
				new_slot_size -= actual_size;
				pde->d_rec_len = actual_size;
				pde = (ext2_dir_entry_t *)((int)pde + pde->d_rec_len);
				pde->d_rec_len = new_slot_size;
				pde->d_inode = 0;
				hit = 1;
				break;
			}

			prev_pde = pde;
		}
		if (hit) break;
        ext2_put_buffer(pb);
	}

	/* the whole directory has been searched */
	if (flag != SD_MAKE) {
		return (flag == SD_IS_EMPTY ? 0 : ENOENT);
	}

	dir_pin->i_last_dpos = pos;
  	dir_pin->i_last_dentry_size = required_space;

    int extended = 0;
    if (!hit) {
        new_slots++;
        /* no free block, create one */
        pb = ext2_new_block(dir_pin, dir_pin->i_size);
        if (!pb) return err_code;

        pde = (ext2_dir_entry_t*)pb->b_data;
        pde->d_rec_len = dir_pin->i_sb->sb_block_size;
        extended = 1;
    }
    
    pde->d_name_len = strlen(string);
    int i;
    for (i = 0; i < pde->d_name_len && string[i]; i++) pde->d_name[i] = string[i];
    pde->d_inode = *num;

    pb->b_dirt = 1;
    ext2_put_buffer(pb);
    dir_pin->i_update |= CTIME | MTIME;	
    dir_pin->i_dirt = 1;

    if (new_slots == 1) {
	    dir_pin->i_size += (off_t) (pde->d_rec_len);
	    if (extended) ext2_rw_inode(dir_pin, DEV_WRITE);
    }
    return 0;
}
