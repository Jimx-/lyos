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
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("ext2fs: "); x
#else
#define DEB(x)
#endif

#define BLOCK2SECTOR(blk_size, blk_nr) (blk_nr * (blk_size))

PUBLIC void init_ext2fs();

PRIVATE int ext2_mountpoint(MESSAGE * p);

/*****************************************************************************
 *                                task_ext2_fs
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK Ext2 FS.
 * 
 *****************************************************************************/
PUBLIC void task_ext2_fs()
{
	printl("ext2fs: Ext2 filesystem driver is running\n");
	init_ext2fs();

	MESSAGE m;

    /* register the filesystem */
    m.type = FS_REGISTER;
    m.PATHNAME = "ext2";
    m.NAME_LEN = strlen(m.PATHNAME);
    send_recv(BOTH, TASK_FS, &m);

	int reply;

	while (1) {
		send_recv(RECEIVE, ANY, &m);

		int msgtype = m.type;
		int src = m.source;
		ext2_pcaller = proc_addr(ENPOINT_P(src));
		reply = 1;

		switch (msgtype) {
		case FS_LOOKUP:
			m.RET_RETVAL = ext2_lookup(&m);
            break;
		case FS_PUTINODE:
            m.RET_RETVAL = ext2_putinode(&m);
			break;
        case FS_MOUNTPOINT:
            m.RET_RETVAL = ext2_mountpoint(&m);
            break;
        case FS_READSUPER:
            m.RET_RETVAL = ext2_readsuper(&m);
            break;
        case FS_STAT:
        	m.STRET = ext2_stat(&m);
        	break;
        case FS_RDWT:
        	m.RWRET = ext2_rdwt(&m);
        	break;
        case FS_CREATE:
        	m.CRRET = ext2_create(&m);
        	break;
        case FS_FTRUNC:
        	m.RET_RETVAL = ext2_ftrunc(&m);
        	break;
        case FS_SYNC:
        	m.RET_RETVAL = ext2_sync();
        	break;
		default:
			dump_msg("ext2: unknown message:", &m);
			break;
		}

		/* reply */
		if (reply) {
			m.type = FSREQ_RET;
			send_recv(SEND, src, &m);
		}

        ext2_sync();
	}
}

PUBLIC void init_ext2fs()
{
    ext2_ep = getpid();
    
	ext2_init_inode();
    ext2_init_buffer_cache();
}

PRIVATE int ext2_mountpoint(MESSAGE * p)
{
    dev_t dev = p->REQ_DEV;
    ino_t num = p->REQ_NUM;

    ext2_inode_t * pin = get_ext2_inode(dev, num);

    if (pin == NULL) return EINVAL;

    if (pin->i_mountpoint) return EBUSY;

    int retval = 0;
    mode_t bits = pin->i_mode & I_TYPE;
    if (bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL) retval =  ENOTDIR;

   put_ext2_inode(pin);

   if (!retval) pin->i_mountpoint = 1;

   return retval;
}

PUBLIC int ext2_sync()
{
	ext2_sync_inodes();
	ext2_sync_buffers();
	return 0;
}
