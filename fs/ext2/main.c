#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/ext2_fs.h"

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("Ext2 FS: "); x
#else
#define DEB(x)
#endif

#define BLOCK2SECTOR(blk_size, blk_nr) (blk_nr * (blk_size)

PUBLIC void init_ext2fs();

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
	MESSAGE msg;

	while (1) {
		
		send_recv(RECEIVE, ANY, &m);

		if (m.type != VFS_REQUEST) phys_copy(&msg, &m, sizeof(m));
		else phys_copy(va2la(getpid(), &msg), va2la(m.source, m.BUF), sizeof(MESSAGE));

		int msgtype = msg.type;
		int src = msg.source;
		pcaller = &proc_table[src];

		switch (msgtype) {
		case OPEN:
			//msg.FD = do_open(&msg);
			break;
		case CLOSE:
			//msg.RETVAL = do_close(&msg);
			break;
		case READ:
		case WRITE:
			//msg.CNT = do_rdwt(&msg);
			break;
		case UNLINK:
			//msg.RETVAL = do_unlink(&msg);
			break;
		case MOUNT:
			//msg.RETVAL = do_mount(&msg);
			break;
		case UMOUNT:
			//msg.RETVAL = do_umount(&msg);
			break;
		case MKDIR:
			//msg.RETVAL = do_mkdir(&msg);
			break;
		case RESUME_PROC:
			//send_recv(SEND, TASK_FS, &msg);
			continue;
			break;
		case FORK:
			//msg.RETVAL = fs_fork(&msg);
			break;
		case EXIT:
			//msg.RETVAL = fs_exit(&msg);
			break;
		case LSEEK:
			//msg.OFFSET = do_lseek(&msg);
			break;
		case STAT:
			//msg.RETVAL = do_stat(&msg);
			break;
		case CHROOT:
			//msg.RETVAL = do_chroot(&msg);
			break;
		case CHDIR:
			//msg.RETVAL = do_chdir(&msg);
			break; 
		default:
			dump_msg("Lyos FS: unknown message:", &msg);
			panic("unknown message.");
			break;
		}

		/* reply */
		if (msg.type != SUSPEND_PROC) {
			msg.type = SYSCALL_RET;
			if (m.type != VFS_REQUEST) send_recv(SEND, src, &msg);
			else send_recv(SEND, TASK_FS, &msg);
		} else {
			send_recv(SEND, TASK_FS, &msg);
		}
	}
}

PUBLIC void init_ext2fs()
{
	ext2_mount_fs(ROOT_DEV);
	printl("ext2fs: Mounted root");
}

PUBLIC void read_ext2_blocks(int dev, int block_nr, int block_count, int proc_nr, void * buf)
{
	if (!block_nr) return;

	struct super_block * psb = get_super_block(dev);
	unsigned long block_size = psb->block_size;

	rw_sector(DEV_READ, dev, block_size * block_nr, block_size * block_count, proc_nr, buf);
}