#ifndef _TMPFS_CONST_H_
#define _TMPFS_CONST_H_

#define TMPFS_ROOT_INODE 1

/* tmpfs_search_dir() flags */
#define SD_LOOK_UP  0 /* tells search_dir to lookup string */
#define SD_MAKE     1 /* tells search_dir to make dir entry */
#define SD_DELETE   2 /* tells search_dir to delete entry */
#define SD_IS_EMPTY 3 /* tells search_dir to check if directory is empty */

/* Inode update flags */
#define ATIME 002 /* set if atime field needs updating */
#define CTIME 004 /* set if ctime field needs updating */
#define MTIME 010 /* set if mtime field needs updating */

#endif
