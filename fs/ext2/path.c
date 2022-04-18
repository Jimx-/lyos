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
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include <sys/stat.h>
#include "ext2_fs.h"
#include "global.h"

// PRIVATE char dot2[2] = "..";

int ext2_lookup(dev_t dev, ino_t start, const char* name,
                struct fsdriver_node* fn, int* is_mountpoint)
{
    ext2_inode_t* dir_pin = find_ext2_inode(dev, start);
    if (!dir_pin) return ENOENT;

    err_code = 0;

    ext2_inode_t* pin = ext2_advance(dir_pin, name);
    if (!pin) return err_code;

    /* fill result */
    fn->fn_num = pin->i_num;
    fn->fn_uid = pin->i_uid;
    fn->fn_gid = pin->i_gid;
    fn->fn_size = pin->i_size;
    fn->fn_mode = pin->i_mode;
    fn->fn_device = pin->i_block[0];
    *is_mountpoint = pin->i_mountpoint;

    return 0;
}

/* find component in dir_pin */
ext2_inode_t* ext2_advance(ext2_inode_t* dir_pin,
                           const char string[EXT2_NAME_LEN + 1])
{
    ino_t num;
    ext2_inode_t* pin;

    /* If 'string' is empty, return an error. */
    if (string[0] == '\0') {
        err_code = ENOENT;
        return NULL;
    }

    /* Check for NULL. */
    if (!dir_pin) return NULL;

    if (dir_pin->i_links_count == 0) {
        err_code = ENOENT;
        return NULL;
    }

    /* If 'string' is not present in the directory, return error. */
    if ((err_code = ext2_search_dir(dir_pin, string, &num, SD_LOOK_UP, 0)) !=
        0) {
        return NULL;
    }

    /* The component has been found in the directory.  Get inode. */
    if ((pin = get_ext2_inode(dir_pin->i_dev, (int)num)) == NULL) {
        return NULL;
    }

    return pin;
}

/* search file named string in dir_pin */
/* if flag == SD_MAKE, create it
 * if flag == SD_DELETE, delete it
 * if flag == SD_LOOK_UP, look it up and return it inode num
 * if flag == SD_IS_EMPTY, check whether this directory is empty */
int ext2_search_dir(ext2_inode_t* dir_pin, const char string[EXT2_NAME_LEN + 1],
                    ino_t* num, int flag, int ftype)
{
    struct fsdriver_buffer* bp;
    ext2_dir_entry_t *pde, *prev_pde;
    loff_t pos;
    block_t b;
    int hit = 0;
    int match = 0;
    unsigned new_slots = 0;
    int required_space = 0;
    int ret = 0;

    if (!S_ISDIR(dir_pin->i_mode)) return ENOTDIR;
    if (flag != SD_IS_EMPTY) {
        if ((strcmp(string, ".") == 0) || (strcmp(string, "..") == 0)) {
            if (flag != SD_LOOK_UP) ret = dir_pin->i_sb->sb_readonly;
        }
    }

    pos = 0;
    if (ret != 0) return ret;

    /* calculate the required space */
    if (flag == SD_MAKE) {
        int len = strlen(string);
        required_space = EXT2_MIN_DIR_ENTRY_SIZE + len;
        required_space +=
            (required_space & 0x03) == 0
                ? 0
                : (EXT2_DIR_ENTRY_ALIGN - (required_space & 0x03));
        if (dir_pin->i_last_dpos < dir_pin->i_size &&
            dir_pin->i_last_dentry_size <= required_space)
            pos = dir_pin->i_last_dpos;
    }

    for (; pos < dir_pin->i_size; pos += dir_pin->i_sb->sb_block_size) {
        b = ext2_read_map(dir_pin, pos);

        if ((ret = fsdriver_get_block(&bp, dir_pin->i_dev, b)) != 0) return ret;
        prev_pde = NULL;

        for (pde = (ext2_dir_entry_t*)bp->data;
             ((char*)pde - (char*)bp->data) < dir_pin->i_sb->sb_block_size;
             pde = (ext2_dir_entry_t*)((char*)pde + pde->d_rec_len)) {

            match = 0;
            if (flag != SD_MAKE && pde->d_inode != (ino_t)0) {
                if (flag == SD_IS_EMPTY) {
                    if (memcmp(pde->d_name, ".", 1) != 0 &&
                        memcmp(pde->d_name, "..", 2) != 0)
                        match = 1; /* dir is not empty */
                } else {
                    if (memcmp(pde->d_name, string, pde->d_name_len) == 0 &&
                        pde->d_name_len == strlen(string)) {
                        match = 1;
                    }
                }
            }

            ret = 0;
            if (match) {
                if (flag == SD_IS_EMPTY)
                    ret = ENOTEMPTY; /* not empty */
                else if (flag == SD_DELETE) {
                    if (pde->d_name_len >= sizeof(ino_t)) {
                        /* Save inode number for recovery. */
                        int ino_offset = pde->d_name_len - sizeof(ino_t);
                        memcpy(&pde->d_name[ino_offset], &pde->d_inode,
                               sizeof(pde->d_inode));
                    }
                    pde->d_inode = (ino_t)0; /* erase entry */
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
                fsdriver_put_block(bp);
                return ret;
            }

            /* Check for free slot */
            if (flag == SD_MAKE && pde->d_inode == 0) {
                /* we found a free slot, check if it has enough space */
                if (required_space <= pde->d_rec_len) {
                    hit = 1; /* we found a free slot */
                    break;
                }
            }

            /* Can we shrink dentry? */
            if (flag == SD_MAKE &&
                required_space <= EXT2_DIR_ENTRY_SHRINK(pde)) {
                int actual_size = EXT2_DIR_ENTRY_ACTUAL_SIZE(pde);
                int new_slot_size = pde->d_rec_len;
                new_slot_size -= actual_size;
                pde->d_rec_len = actual_size;
                pde = (ext2_dir_entry_t*)((char*)pde + pde->d_rec_len);
                pde->d_rec_len = new_slot_size;
                pde->d_inode = 0;
                hit = 1;
                break;
            }

            prev_pde = pde;
        }

        if (hit) break;
        fsdriver_put_block(bp);
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
        bp = ext2_new_block(dir_pin, dir_pin->i_size);
        if (!bp) return err_code;

        pde = (ext2_dir_entry_t*)bp->data;
        pde->d_rec_len = dir_pin->i_sb->sb_block_size;
        extended = 1;
    }

    pde->d_name_len = strlen(string);
    int i;
    for (i = 0; i < pde->d_name_len && string[i]; i++)
        pde->d_name[i] = string[i];
    pde->d_inode = *num;

    fsdriver_mark_dirty(bp);
    fsdriver_put_block(bp);
    dir_pin->i_update |= CTIME | MTIME;
    dir_pin->i_dirt = 1;

    if (new_slots == 1) {
        dir_pin->i_size += (off_t)(pde->d_rec_len);
        if (extended) ext2_rw_inode(dir_pin, WRITE);
    }
    return 0;
}
