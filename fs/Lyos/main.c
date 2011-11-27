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

#include "type.h"
#include "config.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "hd.h"

PRIVATE void mkfs();
PRIVATE int fs_fork();
PRIVATE int fs_exit();

/*****************************************************************************
 *                                task_fs
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK FS.
 * 
 *****************************************************************************/
PUBLIC void task_fs()
{
	init_fs();
	init_buffer();

	/*char * c = get_path("/bin/sh");
	namei(c); */
	while (1) {
		send_recv(RECEIVE, ANY, &fs_msg);

		int msgtype = fs_msg.type;
		int src = fs_msg.source;
		pcaller = &proc_table[src];

		switch (msgtype) {
		case OPEN:
			fs_msg.FD = do_open();
			break;
		case CLOSE:
			fs_msg.RETVAL = do_close();
			break;
		case READ:
		case WRITE:
			fs_msg.CNT = do_rdwt();
			break;
		case UNLINK:
			fs_msg.RETVAL = do_unlink();
			break;
		case MOUNT:
			fs_msg.RETVAL = do_mount();
			break;
		case UMOUNT:
			fs_msg.RETVAL = do_umount();
			break;
		case MKDIR:
			fs_msg.RETVAL = do_mkdir();
			break;
		case RESUME_PROC:
			src = fs_msg.PROC_NR;
			break;
		case FORK:
			fs_msg.RETVAL = fs_fork();
			break;
		case EXIT:
			fs_msg.RETVAL = fs_exit();
			break;
		case LSEEK:
			fs_msg.OFFSET = do_lseek();
			break;
		case STAT:
			fs_msg.RETVAL = do_stat();
			break;
		case CHROOT:
			fs_msg.RETVAL = do_chroot();
			break;
		case CHDIR:
			fs_msg.RETVAL = do_chdir();
			break; 
		default:
			dump_msg("FS::unknown message:", &fs_msg);
			assert(0);
			break;
		}

#ifdef ENABLE_DISK_LOG
		char * msg_name[128];
		msg_name[OPEN]   = "OPEN";
		msg_name[CLOSE]  = "CLOSE";
		msg_name[READ]   = "READ";
		msg_name[WRITE]  = "WRITE";
		msg_name[LSEEK]  = "LSEEK";
		msg_name[UNLINK] = "UNLINK";
		msg_name[MOUNT]  = "MOUNT";
		msg_name[UMOUNT] = "UMOUNT";
		msg_name[MKDIR]  = "MKDIR";
		msg_name[FORK]   = "FORK";
		msg_name[EXIT]   = "EXIT";
		msg_name[STAT]   = "STAT";
		msg_name[CHROOT] = "CHROOT";
		msg_name[CHDIR]  = "CHDIR";

		switch (msgtype) {
		case UNLINK:
		//	dump_fd_graph("%s just finished. (pid:%d)",
		//		      msg_name[msgtype], src);
			//panic("");
		case OPEN:
		case CLOSE:
		case READ:
		case WRITE:
        	case MOUNT:
		case UMOUNT:
		case MKDIR:
		case FORK:
		case EXIT:
		case LSEEK:
		case CHROOT:
		case CHDIR: 
			break;
		case RESUME_PROC:
			break;
		default:
			assert(0);
		}
#endif

		/* reply */
		if (fs_msg.type != SUSPEND_PROC) {
			fs_msg.type = SYSCALL_RET;
			send_recv(SEND, src, &fs_msg);
		}
	}
}

/*****************************************************************************
 *                                init_fs
 *****************************************************************************/
/**
 * <Ring 1> Do some preparation.
 * 
 *****************************************************************************/
PUBLIC void init_fs()
{
	int i;

	/* f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		memset(&f_desc_table[i], 0, sizeof(struct file_desc));

	/* inode_table[] */
	for (i = 0; i < NR_INODE; i++)
		memset(&inode_table[i], 0, sizeof(struct inode));

	/* super_block[] */
	struct super_block * sb = super_block;
	for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
		sb->sb_dev = NO_DEV;

	mount_root();
}

PUBLIC void mount_root()
{
	MESSAGE driver_msg;
	driver_msg.type = DEV_OPEN;
	driver_msg.DEVICE = MINOR(ROOT_DEV);
	assert(dd_map[MAJOR(ROOT_DEV)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(ROOT_DEV)].driver_nr, &driver_msg);

	struct super_block * sb;
	/* read the super block of ROOT DEVICE */
	/*RD_SECT(ROOT_DEV, 1);

	sb = (struct super_block *)fsbuf; */
	
	if(MAJOR(ROOT_DEV) == DEV_FLOPPY){
		printl("Insert root floppy and press ENTER\n");
		while(!key_pressed);
	}
	
	printl("Mkfs on root device...");
	mkfs(); /* make FS */

	/* load super block of ROOT */
	read_super_block(ROOT_DEV);

	sb = get_super_block(ROOT_DEV);
	assert(sb->magic == MAGIC_V1);

	root_inode = get_inode(ROOT_DEV, ROOT_INODE);
	
	int i;
	for (i=0;i<=NR_TASKS;i++){
		proc_table[i].root = root_inode;
		proc_table[i].pwd = root_inode;
	}
	printl("FS: Mounted root(Lyos filesystem)\n");
	
	rd_load_image(ROOT_DEV, 1);
}

/*****************************************************************************
 *                                mkfs
 *****************************************************************************/
/**
 * <Ring 1> Make a available Lyos' FS in the disk. It will
 *          - Write a super block to sector 1.
 *          - Create three special files: dev_tty0, dev_tty1, dev_tty2
 *          - Create a file cmd.tar
 *          - Create the inode map
 *          - Create the sector map
 *          - Create the inodes of the files
 *          - Create `/', the root directory
 *****************************************************************************/
PRIVATE void mkfs()
{
	MESSAGE driver_msg;
	int i, j;

	struct super_block * psb;
	RD_SECT(ROOT_DEV, 1);
	psb = (struct super_block *)fsbuf;
	if (psb->magic == MAGIC_V1){
		printl("an Lyos filesystem found, need no mkfs\n");
		return;
	}
		
	/************************/
	/*      super block     */
	/************************/
	/* get the geometry of ROOTDEV */
	struct part_info geo;
	driver_msg.type		= DEV_IOCTL;
	driver_msg.DEVICE	= MINOR(ROOT_DEV);
	driver_msg.REQUEST	= DIOCTL_GET_GEO;
	driver_msg.BUF		= &geo;
	driver_msg.PROC_NR	= TASK_FS;
	assert(dd_map[MAJOR(ROOT_DEV)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(ROOT_DEV)].driver_nr, &driver_msg);

//	printl("[FS] dev size: 0x%x sectors\n", geo.size);

	int bits_per_sect = SECTOR_SIZE * 8; /* 8 bits per byte */
	/* generate a super block */
	struct super_block sb;
	sb.magic	  = MAGIC_V1; /* 0x111 */
	sb.nr_inodes	  = bits_per_sect;
	sb.nr_inode_sects = sb.nr_inodes * INODE_SIZE / SECTOR_SIZE;
	sb.nr_sects	  = geo.size; /* partition size in sector */
	sb.nr_imap_sects  = 1;
	sb.nr_smap_sects  = sb.nr_sects / bits_per_sect + 1;
	sb.n_1st_sect	  = 1 + 1 +   /* boot sector & super block */
		sb.nr_imap_sects + sb.nr_smap_sects + sb.nr_inode_sects;
	sb.root_inode	  = ROOT_INODE;
	sb.inode_size	  = INODE_SIZE;
	struct inode x;
	sb.inode_isize_off= (int)&x.i_size - (int)&x;
	sb.inode_start_off= (int)&x.i_start_sect - (int)&x;
	sb.dir_ent_size	  = DIR_ENTRY_SIZE;
	struct dir_entry de;
	sb.dir_ent_inode_off = (int)&de.inode_nr - (int)&de;
	sb.dir_ent_fname_off = (int)&de.name - (int)&de;

	memset(fsbuf, 0x90, SECTOR_SIZE);
	memcpy(fsbuf, &sb, SUPER_BLOCK_SIZE);

	/* write the super block */
	WR_SECT(ROOT_DEV, 1);

//	printl("[FS] devbase:0x%x00, sb:0x%x00, imap:0x%x00, smap:0x%x00\n"
//	       "        inodes:0x%x00, 1st_sector:0x%x00\n", 
//	       geo.base * 2,
//	       (geo.base + 1) * 2,
//	       (geo.base + 1 + 1) * 2,
//	       (geo.base + 1 + 1 + sb.nr_imap_sects) * 2,
//	       (geo.base + 1 + 1 + sb.nr_imap_sects + sb.nr_smap_sects) * 2,
//	       (geo.base + sb.n_1st_sect) * 2);

	/************************/
	/*       inode map      */
	/************************/
	memset(fsbuf, 0, SECTOR_SIZE);
	for (i = 0; i < (NR_CONSOLES + 4); i++)
		fsbuf[0] |= 1 << i;

	assert(fsbuf[0] == 0x7F);/* 0111 1111 :
				              *  ||| |||`--- bit 0 : reserved
				              *  ||| ||`---- bit 1 : the first inode,
				              *  ||| ||              which indicates `/'
				              *  ||| |`----- bit 2 : /dev_tty0
				              *  ||| `------ bit 3 : /dev_tty1
				              *  ||`-------- bit 4 : /dev_tty2
				              *  |`--------- bit 5 : /userlist
				              *  `---------- bit 6 : /cmd.tar
							  */
	WR_SECT(ROOT_DEV, 2);

	/************************/
	/*      secter map      */
	/************************/
	memset(fsbuf, 0, SECTOR_SIZE);
	int nr_sects = NR_DEFAULT_FILE_SECTS + 1;
	/*             ~~~~~~~~~~~~~~~~~~~|~   |
	 *                                |    `--- bit 0 is reserved
	 *                                `-------- for `/'
	 */
	for (i = 0; i < nr_sects / 8; i++)
		fsbuf[i] = 0xFF;

	for (j = 0; j < nr_sects % 8; j++)
		fsbuf[i] |= (1 << j);

	WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects);

	/* zeromemory the rest sector-map */
	memset(fsbuf, 0, SECTOR_SIZE);
	for (i = 1; i < sb.nr_smap_sects; i++)
		WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + i);

	/* cmd.tar */
	/* make sure it'll not be overwritten by the disk log */
	assert(INSTALL_START_SECT + INSTALL_NR_SECTS < 
	       sb.nr_sects - NR_SECTS_FOR_LOG);
	int bit_offset = INSTALL_START_SECT -
		sb.n_1st_sect + 1; /* sect M <-> bit (M - sb.n_1stsect + 1) */
	int bit_off_in_sect = bit_offset % (SECTOR_SIZE * 8);
	int bit_left = INSTALL_NR_SECTS;
	int cur_sect = bit_offset / (SECTOR_SIZE * 8);
	RD_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
	while (bit_left) {
		int byte_off = bit_off_in_sect / 8;
		/* this line is ineffecient in a loop, but I don't care */
		fsbuf[byte_off] |= 1 << (bit_off_in_sect % 8);
		bit_left--;
		bit_off_in_sect++;
		if (bit_off_in_sect == (SECTOR_SIZE * 8)) {
			WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
			cur_sect++;
			RD_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
			bit_off_in_sect = 0;
		}
	}
	WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);

	/************************/
	/*       inodes         */
	/************************/
	/* inode of `/' */
	memset(fsbuf, 0, SECTOR_SIZE);
	struct inode * pi = (struct inode*)fsbuf;
	pi->i_mode = I_DIRECTORY;
	pi->i_size = DIR_ENTRY_SIZE * 7; /* 6 files:
					  * `.',
					  * `dev_tty0', `dev_tty1', `dev_tty2',
					  * `userlist', `cmd.tar'
					  */
	pi->i_start_sect = sb.n_1st_sect;
	pi->i_nr_sects = NR_DEFAULT_FILE_SECTS;
	/* inode of `/dev_tty0~2' */
	for (i = 0; i < NR_CONSOLES; i++) {
		pi = (struct inode*)(fsbuf + (INODE_SIZE * (i + 1)));
		pi->i_mode = I_CHAR_SPECIAL;
		pi->i_size = 0;
		pi->i_start_sect = MAKE_DEV(DEV_CHAR_TTY, i);
		pi->i_nr_sects = 0;
	}
	pi = (struct inode*)(fsbuf + (INODE_SIZE * (NR_CONSOLES + 1)));
	pi->i_mode = I_REGULAR;
	pi->i_size = 64;
	pi->i_start_sect = MAKE_DEV(DEV_HD, 1);
	pi->i_nr_sects = NR_DEFAULT_FILE_SECTS;
	/* inode of `/cmd.tar' */
	pi = (struct inode*)(fsbuf + (INODE_SIZE * (NR_CONSOLES + 2)));
	pi->i_mode = I_REGULAR;
	pi->i_size = INSTALL_NR_SECTS * SECTOR_SIZE;
	pi->i_start_sect = INSTALL_START_SECT;
	pi->i_nr_sects = INSTALL_NR_SECTS;

	WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + sb.nr_smap_sects);

	/************************/
	/*          `/'         */
	/************************/
	memset(fsbuf, 0, SECTOR_SIZE);
	struct dir_entry * pde = (struct dir_entry *)fsbuf;

	pde->inode_nr = 1;
	strcpy(pde->name, ".");

	/* dir entries of `/dev_tty0~2' */
	for (i = 0; i < NR_CONSOLES; i++) {
		pde++;
		pde->inode_nr = i + 2; /* dev_tty0's inode_nr is 2 */
		sprintf(pde->name, "dev_tty%d", i);
	}
	(++pde)->inode_nr = NR_CONSOLES + 2;
	sprintf(pde->name, "root", i);
	WR_SECT(ROOT_DEV, sb.n_1st_sect);
	(++pde)->inode_nr = NR_CONSOLES + 3;
	sprintf(pde->name, "cmd.tar", i);
	WR_SECT(ROOT_DEV, sb.n_1st_sect);
	printl("done.");
}

/*****************************************************************************
 *                                rw_sector
 *****************************************************************************/
/**
 * <Ring 1> R/W a sector via messaging with the corresponding driver.
 * 
 * @param io_type  DEV_READ or DEV_WRITE
 * @param dev      device nr
 * @param pos      Byte offset from/to where to r/w.
 * @param bytes    r/w count in bytes.
 * @param proc_nr  To whom the buffer belongs.
 * @param buf      r/w buffer.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int rw_sector(int io_type, int dev, u64 pos, int bytes, int proc_nr,
		     void* buf)
{
	MESSAGE driver_msg;

	driver_msg.type		= io_type;
	driver_msg.DEVICE	= MINOR(dev);
	driver_msg.POSITION	= pos;
	driver_msg.BUF		= buf;
	driver_msg.CNT		= bytes;
	driver_msg.PROC_NR	= proc_nr;
	assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);

	return 0;
}


/*****************************************************************************
 *                                read_super_block
 *****************************************************************************/
/**
 * <Ring 1> Read super block from the given device then write it into a free
 *          super_block[] slot.
 * 
 * @param dev  From which device the super block comes.
 *****************************************************************************/
PUBLIC void read_super_block(int dev)
{
	int i;
	MESSAGE driver_msg;

	driver_msg.type		= DEV_READ;
	driver_msg.DEVICE	= MINOR(dev);
	driver_msg.POSITION	= SECTOR_SIZE * 1;
	driver_msg.BUF		= fsbuf;
	driver_msg.CNT		= SECTOR_SIZE;
	driver_msg.PROC_NR	= TASK_FS;
	assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);

	/* find a free slot in super_block[] */
	for (i = 0; i < NR_SUPER_BLOCK; i++)
		if (super_block[i].sb_dev == NO_DEV)
			break;
	if (i == NR_SUPER_BLOCK)
		panic("super_block slots used up");

	struct super_block * psb = (struct super_block *)fsbuf;

	super_block[i] = *psb;
	super_block[i].sb_dev = dev;
}


/*****************************************************************************
 *                                get_super_block
 *****************************************************************************/
/**
 * <Ring 1> Get the super block from super_block[].
 * 
 * @param dev Device nr.
 * 
 * @return Super block ptr.
 *****************************************************************************/
PUBLIC struct super_block * get_super_block(int dev)
{
	struct super_block * sb = super_block;
	for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
		if (sb->sb_dev == dev)
			return sb;

	panic("super block of device %d not found.\n", dev);

	return 0;
}


/*****************************************************************************
 *                                get_inode
 *****************************************************************************/
/**
 * <Ring 1> Get the inode ptr of given inode nr. A cache -- inode_table[] -- is
 * maintained to make things faster. If the inode requested is already there,
 * just return it. Otherwise the inode will be read from the disk.
 * 
 * @param dev Device nr.
 * @param num I-node nr.
 * 
 * @return The inode ptr requested.
 *****************************************************************************/
PUBLIC struct inode * get_inode(int dev, int num)
{
	if (num == 0)
		return 0;

	struct inode * p;
	struct inode * q = 0;
	for (p = &inode_table[0]; p < &inode_table[NR_INODE]; p++) {
		if (p->i_cnt) {	/* not a free slot */
			if ((p->i_dev == dev) && (p->i_num == num)) {
				/* this is the inode we want */
				p->i_cnt++;
				return p;
			}
		}
		else {		/* a free slot */
			if (!q) /* q hasn't been assigned yet */
				q = p; /* q <- the 1st free slot */
		}
	}

	if (!q)
		panic("the inode table is full");

	q->i_dev = dev;
	q->i_num = num;
	q->i_cnt = 1;

	struct super_block * sb = get_super_block(dev);
	int blk_nr = 1 + 1 + sb->nr_imap_sects + sb->nr_smap_sects +
		((num - 1) / (SECTOR_SIZE / INODE_SIZE));
	RD_SECT(dev, blk_nr);
	struct inode * pinode =
		(struct inode*)((u8*)fsbuf +
				((num - 1 ) % (SECTOR_SIZE / INODE_SIZE))
				 * INODE_SIZE);
	q->i_mode = pinode->i_mode;
	q->i_size = pinode->i_size;
	q->i_start_sect = pinode->i_start_sect;
	q->i_nr_sects = pinode->i_nr_sects;
	return q;
}

/*****************************************************************************
 *                                put_inode
 *****************************************************************************/
/**
 * Decrease the reference nr of a slot in inode_table[]. When the nr reaches
 * zero, it means the inode is not used any more and can be overwritten by
 * a new inode.
 * 
 * @param pinode I-node ptr.
 *****************************************************************************/
PUBLIC void put_inode(struct inode * pinode)
{
	assert(pinode->i_cnt > 0);
	pinode->i_cnt--;
}

/*****************************************************************************
 *                                sync_inode
 *****************************************************************************/
/**
 * <Ring 1> Write the inode back to the disk. Commonly invoked as soon as the
 *          inode is changed.
 * 
 * @param p I-node ptr.
 *****************************************************************************/
PUBLIC void sync_inode(struct inode * p)
{
	struct inode * pinode;
	struct super_block * sb = get_super_block(p->i_dev);
	int blk_nr = 1 + 1 + sb->nr_imap_sects + sb->nr_smap_sects +
		((p->i_num - 1) / (SECTOR_SIZE / INODE_SIZE));
	RD_SECT(p->i_dev, blk_nr);
	pinode = (struct inode*)((u8*)fsbuf +
				 (((p->i_num - 1) % (SECTOR_SIZE / INODE_SIZE))
				  * INODE_SIZE));
	pinode->i_mode = p->i_mode;
	pinode->i_size = p->i_size;
	pinode->i_start_sect = p->i_start_sect;
	pinode->i_nr_sects = p->i_nr_sects;
	WR_SECT(p->i_dev, blk_nr);
}

/*****************************************************************************
 *                                fs_fork
 *****************************************************************************/
/**
 * Perform the aspects of fork() that relate to files.
 * 
 * @return Zero if success, otherwise a negative integer.
 *****************************************************************************/
PRIVATE int fs_fork()
{
	int i;
	struct proc* child = &proc_table[fs_msg.PID];
	for (i = 0; i < NR_FILES; i++) {
		if (child->filp[i]) {
			child->filp[i]->fd_cnt++;
			child->filp[i]->fd_inode->i_cnt++;
		}
	}

	return 0;
}


/*****************************************************************************
 *                                fs_exit
 *****************************************************************************/
/**
 * Perform the aspects of exit() that relate to files.
 * 
 * @return Zero if success.
 *****************************************************************************/
PRIVATE int fs_exit()
{
	int i;
	struct proc* p = &proc_table[fs_msg.PID];
	for (i = 0; i < NR_FILES; i++) {
		if (p->filp[i]) {
			/* release the inode */
			p->filp[i]->fd_inode->i_cnt--;
			/* release the file desc slot */
			if (--p->filp[i]->fd_cnt == 0)
				p->filp[i]->fd_inode = 0;
			p->filp[i] = 0;
		}
	}
	return 0;
}




