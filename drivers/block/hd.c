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
#include "stdio.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/driver.h"
#include <lyos/portio.h>

PRIVATE void	init_hd				();
PRIVATE int 	hd_open				(MESSAGE * p);
PRIVATE int 	hd_close			(MESSAGE * p);
PRIVATE int 	hd_rdwt				(MESSAGE * p);
PRIVATE int 	hd_ioctl			(MESSAGE * p);
PRIVATE void	hd_cmd_out			(struct hd_cmd* cmd);
PRIVATE void	get_part_table		(int drive, int sect_nr, struct part_ent * entry);
PRIVATE void	partition			(int device, int style);
PRIVATE void	print_hdinfo		(struct hd_info * hdi); 
PRIVATE int		waitfor				(int mask, int val, int timeout);
PRIVATE void	interrupt_wait		();
PRIVATE	void	hd_identify			(int drive);
PRIVATE void	print_identify_info	(u16* hdinfo);
PRIVATE void 	register_hd(struct hd_info * hdi);
PUBLIC 	int 	hd_handler(irq_hook_t * hook);

PRIVATE	u8		hd_status;
PRIVATE	u8		hdbuf[SECTOR_SIZE * 2];
PRIVATE	struct hd_info	hd_info[1];

PRIVATE irq_hook_t hd_hook;

#define	DRV_OF_DEV(dev) (dev / NR_SUB_PER_DRIVE)

//#define HDDEBUG
#ifdef HDDEBUG
#define DEB(x) printl("HD: "); x
#else
#define DEB(x)
#endif

struct dev_driver hd_driver = 
{
	"HD",
	hd_open,
	hd_close,
	hd_rdwt,
	hd_rdwt, 
	hd_ioctl 
};

/*****************************************************************************
 *                                task_hd
 *****************************************************************************/
/**
 * Main loop of HD driver.
 * 
 *****************************************************************************/
PUBLIC void task_hd()
{
	init_hd();
	dev_driver_task(&hd_driver);	

	
	/*while (1) {
		send_recv(RECEIVE, ANY, &msg);
		DEB(printl("Receive a message from %d, type = %d\n", msg.source, msg.type));
		int src = msg.source;

		switch (msg.type) {
		case DEV_OPEN:
			hd_open(&msg);
			break;

		case DEV_CLOSE:
			hd_close(&msg);
			break;

		case DEV_READ:
		case DEV_WRITE:
			hd_rdwt(&msg);
			break;

		case DEV_IOCTL:
			hd_ioctl(&msg);
			break;

		default:
			dump_msg("HD driver: Unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
			break;
		}

		send_recv(SEND, src, &msg);
	} */

}

/*****************************************************************************
 *                                init_hd
 *****************************************************************************/
/**
 * <Ring 1> Check hard drive, set IRQ handler, enable IRQ and initialize data
 *          structures.
 *****************************************************************************/
PRIVATE void init_hd()
{
	int i;
	/* Get the number of drives from the BIOS data area */
	u8 * pNrDrives = (u8*)(0x475 + KERNEL_VMA);
	printl("hd: %d hard drives\n", *pNrDrives);
	assert(*pNrDrives);

	put_irq_handler(AT_WINI_IRQ, &hd_hook, hd_handler);
	enable_irq(CASCADE_IRQ);
	enable_irq(AT_WINI_IRQ);

	for (i = 0; i < (sizeof(hd_info) / sizeof(hd_info[0])); i++)
		memset(&hd_info[i], 0, sizeof(hd_info[0]));
	hd_info[0].open_cnt = 0;

	for (i = 0; i < *pNrDrives; i++) {
		hd_identify(i);

		if (hd_info[i].open_cnt++ == 0) {
			partition(i * NR_SUB_PER_DRIVE, P_PRIMARY);
			print_hdinfo(&hd_info[i]);
			register_hd(&hd_info[i]);
		}
	}
}

/*****************************************************************************
 *                                hd_open
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_OPEN message. It identify the drive
 * of the given device and read the partition table of the drive if it
 * has not been read.
 * 
 * @param device The device to be opened.
 *****************************************************************************/
PRIVATE int hd_open(MESSAGE * p)
{
	int drive = DRV_OF_DEV(p->DEVICE);
	assert(drive == 0);	/* only one drive */

	hd_info[drive].open_cnt++;

	return 0;
}

/*****************************************************************************
 *                                hd_close
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_CLOSE message. 
 * 
 * @param device The device to be opened.
 *****************************************************************************/
PRIVATE int hd_close(MESSAGE * p)
{
	int drive = DRV_OF_DEV(p->DEVICE);
	assert(drive == 0);	/* only one drive */

	hd_info[drive].open_cnt--;

	return 0;
}

/*****************************************************************************
 *                                hd_rdwt
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_READ and DEV_WRITE message.
 * 
 * @param p Message ptr.
 *****************************************************************************/
PRIVATE int hd_rdwt(MESSAGE * p)
{
	int drive = DRV_OF_DEV(p->DEVICE);
			
	u64 pos = p->POSITION;
	assert((pos >> SECTOR_SIZE_SHIFT) < (1 << 31));

	/**
	 * We only allow to R/W from a SECTOR boundary:
	 */
	assert((pos & 0x1FF) == 0);

	u32 sect_nr = (u32)(pos >> SECTOR_SIZE_SHIFT); /* pos / SECTOR_SIZE */
	int part_no = MINOR(p->DEVICE) % NR_SUB_PER_DRIVE;
	sect_nr += part_no < NR_PRIM_PER_DRIVE ?
		hd_info[drive].primary[part_no].base :
		hd_info[drive].logical[part_no - NR_PRIM_PER_DRIVE].base;

	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= (p->CNT + SECTOR_SIZE - 1) / SECTOR_SIZE;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, drive, (sect_nr >> 24) & 0xF);
	cmd.command	= (p->type == DEV_READ) ? ATA_READ : ATA_WRITE;
	hd_cmd_out(&cmd);

	int bytes_left = p->CNT;
	void * la = (void*)va2la(p->PROC_NR, p->BUF);
	int buf = (int)p->BUF;

	while (bytes_left) {
		int bytes = min(SECTOR_SIZE, bytes_left);
		if (p->type == DEV_READ) {
			interrupt_wait();
			port_read(REG_DATA, hdbuf, SECTOR_SIZE);
			data_copy(p->PROC_NR, (void*)buf, TASK_HD, hdbuf, bytes);
			//phys_copy(la, (void*)va2la(TASK_HD, hdbuf), bytes);
		}
		else {
			if (!waitfor(STATUS_DRQ, STATUS_DRQ, HD_TIMEOUT))
				panic("hd writing error.");

			data_copy(TASK_HD, hdbuf, p->PROC_NR, (void*)buf, bytes);
			port_write(REG_DATA, hdbuf, bytes);
			interrupt_wait();
		}
		bytes_left -= SECTOR_SIZE;
		buf += SECTOR_SIZE;
		la += SECTOR_SIZE;
	}
	return 0;
}				

/*****************************************************************************
 *                                hd_ioctl
 *****************************************************************************/
/**
 * <Ring 1> This routine handles the DEV_IOCTL message.
 * 
 * @param p  Ptr to the MESSAGE.
 *****************************************************************************/
PRIVATE int hd_ioctl(MESSAGE * p)
{
	int device = p->DEVICE;
	int drive = DRV_OF_DEV(device);

	struct hd_info * hdi = &hd_info[drive];

	if (p->REQUEST == DIOCTL_GET_GEO) {
		//void * dst = va2la(p->PROC_NR, p->BUF);
		int part_no = MINOR(device);
		/*void * src = va2la(TASK_HD,
				   part_no < NR_PRIM_PER_DRIVE ?
				   &hdi->primary[part_no] :
				   &hdi->logical[part_no - NR_PRIM_PER_DRIVE]); */

		data_copy(p->PROC_NR, p->BUF, TASK_HD, part_no < NR_PRIM_PER_DRIVE ?
				   &hdi->primary[part_no] :
				   &hdi->logical[part_no - NR_PRIM_PER_DRIVE], sizeof(struct part_info));
		//phys_copy(dst, src, sizeof(struct part_info));
	}
	else {
		assert(0);
	}

	return 0;
}

/*****************************************************************************
 *                                get_part_table
 *****************************************************************************/
/**
 * <Ring 1> Get a partition table of a drive.
 * 
 * @param drive   Drive nr (0 for the 1st disk, 1 for the 2nd, ...)n
 * @param sect_nr The sector at which the partition table is located.
 * @param entry   Ptr to part_ent struct.
 *****************************************************************************/
PRIVATE void get_part_table(int drive, int sect_nr, struct part_ent * entry)
{
	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= 1;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, /* LBA mode*/
					  drive,
					  (sect_nr >> 24) & 0xF);
	cmd.command	= ATA_READ;
	hd_cmd_out(&cmd);
	interrupt_wait();

	port_read(REG_DATA, hdbuf, SECTOR_SIZE);
	memcpy(entry,
	       hdbuf + PARTITION_TABLE_OFFSET,
	       sizeof(struct part_ent) * NR_PART_PER_DRIVE);
}

/*****************************************************************************
 *                                partition
 *****************************************************************************/
/**
 * <Ring 1> This routine is called when a device is opened. It reads the
 * partition table(s) and fills the hd_info struct.
 * 
 * @param device Device nr.
 * @param style  P_PRIMARY or P_EXTENDED.
 *****************************************************************************/
PRIVATE void partition(int device, int style)
{
	int i;
	int drive = DRV_OF_DEV(device);
	struct hd_info * hdi = &hd_info[drive];

	struct part_ent part_tbl[NR_SUB_PER_DRIVE];

	if (style == P_PRIMARY) {
		get_part_table(drive, drive, part_tbl);

		int nr_prim_parts = 0;
		for (i = 0; i < NR_PART_PER_DRIVE; i++) { /* 0~3 */
			if (part_tbl[i].sys_id == NO_PART) 
				continue;

			nr_prim_parts++;
			int dev_nr = i + 1;		  /* 1~4 */
			hdi->primary[dev_nr].base = part_tbl[i].start_sect;
			hdi->primary[dev_nr].size = part_tbl[i].nr_sects;

			if (part_tbl[i].sys_id == EXT_PART) /* extended */
				partition(device + dev_nr, P_EXTENDED);
		}
		assert(nr_prim_parts != 0);
	}
	else if (style == P_EXTENDED) {
		int j;
		/* find first free slot for logical partition */
		for (j = 0; j < NR_SUB_PER_DRIVE; j++) {
			if (hdi->logical[j].size == 0) break;
		}
		int k = device % NR_PRIM_PER_DRIVE; /* 1~4 */
		int ext_start_sect = hdi->primary[k].base;
		int s = ext_start_sect;
		int nr_1st_sub = j;

		for (i = 0; i < NR_SUB_PER_PART; i++) {
			int dev_nr = nr_1st_sub + i;

			get_part_table(drive, s, part_tbl);

			hdi->logical[dev_nr].base = s + part_tbl[0].start_sect;
			hdi->logical[dev_nr].size = part_tbl[0].nr_sects;

			s = ext_start_sect + part_tbl[1].start_sect;

			/* no more logical partitions
			   in this extended partition */
			if (part_tbl[1].sys_id == NO_PART)
				break;
		}
	}
	else {
		assert(0);
	}
}

/*****************************************************************************  */
/*                                print_hdinfo									*/
/*****************************************************************************  */
/* <Ring 1> Print disk info.													*/
/*  *																			*/
/*  * @param hdi  Ptr to struct hd_info.										*/
/*  *****************************************************************************/
PRIVATE void print_hdinfo(struct hd_info * hdi) 
 { 
 	int i; 
 	printl("Hard disk information:\n");
	for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) { 
		if (hdi->primary[i].size == 0) 
			continue; 
 		printl("%spartition %d: base %d(0x%x), size %d(0x%x) (in sector)\n", 
	       i == 0 ? "  " : "     ", 
		       i, 
 		       hdi->primary[i].base, 
		       hdi->primary[i].base, 
		       hdi->primary[i].size, 
		       hdi->primary[i].size); 
	} 
	for (i = 0; i < NR_SUB_PER_DRIVE; i++) { 
		if (hdi->logical[i].size == 0) 
			continue; 
		printl("              "
		       "%d: base %d(0x%x), size %d(0x%x) (in sector)\n", 
		       i, 
 		       hdi->logical[i].base, 
		       hdi->logical[i].base, 
		       hdi->logical[i].size, 
 		       hdi->logical[i].size); 
 	} 
} 

/*****************************************************************************  */
/*                                register_hd									*/
/*****************************************************************************  */
PRIVATE void register_hd(struct hd_info * hdi) 
 { 
 	int i, drive = (hdi - hd_info) + 1; 
	for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) { 
		if (hdi->primary[i].size == 0) 
			continue; 
		char name[6];
		memset(name, 0, sizeof(name));

		if (i == 0) sprintf(name, "hd%d", drive);
		else sprintf(name, "hd%d%c", drive, 'a' + i - 1);

		announce_blockdev(name, MAKE_DEV(DEV_HD, (drive - 1) * NR_SUB_PER_DRIVE + i));
	} 
	for (i = 0; i < NR_SUB_PER_DRIVE; i++) { 
		if (hdi->logical[i].size == 0) 
			continue; 
		char name[6];
		memset(name, 0, sizeof(name));

		sprintf(name, "hd%d%c", drive, 'a' + i + NR_PRIM_PER_DRIVE - 1);

		announce_blockdev(name, MAKE_DEV(DEV_HD, (drive - 1) * NR_SUB_PER_DRIVE + NR_PRIM_PER_DRIVE + i));
 	} 
} 

/*****************************************************************************
/ *                                hd_identify								 *
* ****************************************************************************/
/**
 * <Ring 1> Get the disk information.
 * 
 * @param drive  Drive Nr.
 *****************************************************************************/
PRIVATE void hd_identify(int drive)
{
	struct hd_cmd cmd;
	cmd.device  = MAKE_DEVICE_REG(0, drive, 0);
	cmd.command = ATA_IDENTIFY;
	hd_cmd_out(&cmd);
	interrupt_wait();
	port_read(REG_DATA, hdbuf, SECTOR_SIZE);

	u16* hdinfo = (u16*)hdbuf;
	printl("hd%d: ", drive);
    print_identify_info(hdinfo);

	hd_info[drive].primary[0].base = 0;
	/* Total Nr of User Addressable Sectors */
	hd_info[drive].primary[0].size = ((int)hdinfo[61] << 16) + hdinfo[60];
}

/*****************************************************************************
 *                            print_identify_info
 *****************************************************************************/
/**
 * <Ring 1> Print the hdinfo retrieved via ATA_IDENTIFY command.
 * 
 * @param hdinfo  The buffer read from the disk i/o port.
 *****************************************************************************/
PRIVATE void print_identify_info(u16* hdinfo)
{
	int i, j, k;
	char s[64];

	struct iden_info_ascii {
		int idx;
		int len;
		char * desc;
	} iinfo[] = {{27, 40, "HD Model: "}, /* Serial number in ASCII */
		     {10, 20, "HD Serial Number: "} /* Model number in ASCII */ };

	int sectors = ((int)hdinfo[61] << 16) + hdinfo[60];
	printl("%dMB ", sectors * 512 / 1000000);

	for (k = 0; k < sizeof(iinfo)/sizeof(iinfo[0]); k++) {
		char * p = (char*)&hdinfo[iinfo[k].idx];
		for (i = 0; i < iinfo[k].len/2; i++) {
			s[i*2+1] = *p++;
			s[i*2] = *p++;
		}
		s[i*2] = 0;
		/*printl(iinfo[k].desc);*/
		for (j = 0;j <= i*2; j++)
			if (s[j] != ' ') printl("%c", s[j]);
		printl(" ");
	}

	int capabilities = hdinfo[49];
	if (capabilities & 0x0200) printl("LBA ");

	int cmd_set_supported = hdinfo[83];
	if (cmd_set_supported & 0x0400) printl("LBA48 ");

	printl("\n");
}

/*****************************************************************************
 *                                hd_cmd_out
 *****************************************************************************/
/**
 * <Ring 1> Output a command to HD controller.
 * 
 * @param cmd  The command struct ptr.
 *****************************************************************************/
PRIVATE void hd_cmd_out(struct hd_cmd* cmd)
{
	/**
	 * For all commands, the host must first check if BSY=1,
	 * and should proceed no further unless and until BSY=0
	 */
	if (!waitfor(STATUS_BSY, 0, HD_TIMEOUT))
		panic("hd error.");

	pb_pair_t cmd_pairs[8];
	/* Activate the Interrupt Enable (nIEN) bit */
	pv_set(cmd_pairs[0], REG_DEV_CTRL, 0);
	/* Load required parameters in the Command Block Registers */
	pv_set(cmd_pairs[1], REG_FEATURES, cmd->features);
	pv_set(cmd_pairs[2], REG_NSECTOR, cmd->count);
	pv_set(cmd_pairs[3], REG_LBA_LOW, cmd->lba_low);
	pv_set(cmd_pairs[4], REG_LBA_MID, cmd->lba_mid);
	pv_set(cmd_pairs[5], REG_LBA_HIGH, cmd->lba_high);
	pv_set(cmd_pairs[6], REG_DEVICE, cmd->device);
	/* Write the command code to the Command Register */
	pv_set(cmd_pairs[7], REG_CMD, cmd->command);
	portio_voutb(cmd_pairs, 8);

	/* Activate the Interrupt Enable (nIEN) bit */
	//portio_outb(REG_DEV_CTRL, 0);
	/* Load required parameters in the Command Block Registers */
	/*portio_outb(REG_FEATURES, cmd->features);
	portio_outb(REG_NSECTOR,  cmd->count);
	portio_outb(REG_LBA_LOW,  cmd->lba_low);
	portio_outb(REG_LBA_MID,  cmd->lba_mid);
	portio_outb(REG_LBA_HIGH, cmd->lba_high);
	portio_outb(REG_DEVICE,   cmd->device); */
	/* Write the command code to the Command Register */
	//portio_outb(REG_CMD,     cmd->command);
}

/*****************************************************************************
 *                                interrupt_wait
 *****************************************************************************/
/**
 * <Ring 1> Wait until a disk interrupt occurs.
 * 
 *****************************************************************************/
PRIVATE void interrupt_wait()
{
	MESSAGE msg;
	send_recv(RECEIVE, INTERRUPT, &msg);
}

/*****************************************************************************
 *                                waitfor
 *****************************************************************************/
/**
 * <Ring 1> Wait for a certain status.
 * 
 * @param mask    Status mask.
 * @param val     Required status.
 * @param timeout Timeout in milliseconds.
 * 
 * @return One if sucess, zero if timeout.
 *****************************************************************************/
PRIVATE int waitfor(int mask, int val, int timeout)
{
	int t = get_ticks();

	while(((get_ticks() - t) * 1000 / HZ) < timeout) {
		u8 status;
		portio_inb(REG_STATUS, &status);
		if ((status & mask) == val)
			return 1;
	}

	return 0;
}

/*****************************************************************************
 *                                hd_handler
 *****************************************************************************/
/**
 * <Ring 0> Interrupt handler.
 * 
 * @param irq  IRQ nr of the disk interrupt.
 *****************************************************************************/
PUBLIC int hd_handler(irq_hook_t * hook)
{
	/*
	 * Interrupts are cleared when the host
	 *   - reads the Status Register,
	 *   - issues a reset, or
	 *   - writes to the Command Register.
	 */
	hd_status = in_byte(REG_STATUS);

	inform_int(TASK_HD);

	return 1;
}
