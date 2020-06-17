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

#ifndef _ATA_H_
#define _ATA_H_

#include <libdevman/libdevman.h>

#define PCI_DMA_2ND_OFF 8

#define REG_CMD_BASE0 0x1F0 /* command base register of controller 0 */
#define REG_CMD_BASE1 0x170 /* command base register of controller 1 */
#define REG_CTL_BASE0 0x3F6 /* control base register of controller 0 */
#define REG_CTL_BASE1 0x376 /* control base register of controller 1 */

/********************************************/
/* I/O Ports used by hard disk controllers. */
/********************************************/
/* slave disk not supported yet, all master registers below */

/* Command Block Registers */
/*	MACRO		PORT			DESCRIPTION			INPUT/OUTPUT	*/
/*	-----		----			-----------			------------	*/
#define REG_DATA 0             /*	Data				I/O		*/
#define REG_FEATURES 1         /*	Features			O		*/
#define REG_ERROR REG_FEATURES /*	Error				I		*/
/*  The contents of this register are valid only when the error bit
    (ERR) in the Status Register is set, except at drive power-up or at the
    completion of the drive's internal diagnostics, when the register
    contains a status code.
    When the error bit (ERR) is set, Error Register bits are interpreted as
   such: |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
    +-----+-----+-----+-----+-----+-----+-----+-----+
    | BRK | UNC |     | IDNF|     | ABRT|TKONF| AMNF|
    +-----+-----+-----+-----+-----+-----+-----+-----+
       |     |     |     |     |     |     |     |
       |     |     |     |     |     |     |     `--- 0. Data address mark not
   found after correct ID field found |     |     |     |     |     |
   `--------- 1. Track 0 not found during execution of Recalibrate command | |
   |     |     |     `--------------- 2. Command aborted due to drive status
   error or invalid command |     |     |     |     `--------------------- 3.
   Not used |     |     |     `--------------------------- 4. Requested sector's
   ID field not found |     |     `--------------------------------- 5. Not used
       |     `--------------------------------------- 6. Uncorrectable data
   error encountered
       `--------------------------------------------- 7. Bad block mark detected
   in the requested sector's ID field
*/

#define ERROR_BAD_BLOCK 0x80

#define REG_NSECTOR 2  /*	Sector Count			I/O		*/
#define REG_LBA_LOW 3  /*	Sector Number / LBA Bits 0-7	I/O		*/
#define REG_LBA_MID 4  /*	Cylinder Low / LBA Bits 8-15	I/O		*/
#define REG_LBA_HIGH 5 /*	Cylinder High / LBA Bits 16-23	I/O		*/
#define REG_DEVICE 6   /*	Drive | Head | LBA bits 24-27	I/O		*/
                       /*	|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
                           +-----+-----+-----+-----+-----+-----+-----+-----+
                           |  1  |  L  |  1  | DRV | HS3 | HS2 | HS1 | HS0 |
                           +-----+-----+-----+-----+-----+-----+-----+-----+
                                    |           |   \_____________________/
                                    |           |              |
                                    |           |              `------------ If L=0, Head Select.
                                    |           |                                    These four bits
                          select the head number.                        |           |                        HS0                        is the least
                          significant.                        |           |                        If
                          L=1,                        HS0 through HS3                        contain
                          bit 24-27 of the LBA.                        |
                          `---------------------------                        Drive. When DRV=0, drive
                          0 (master) is selected.                        |                        When
                          DRV=1, drive 1                        (slave)                        is
                          selected.
                                    `--------------------------------------- LBA mode. This bit selects
                          the mode of operation.                        When L=0, addressing is by
                          'CHS' mode.                        When L=1,                        addressing is by 'LBA' mode.
                       */
#define REG_STATUS 7   /*	Status				I		*/
/*  Any pending interrupt is cleared whenever this register is read.
    |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
    +-----+-----+-----+-----+-----+-----+-----+-----+
    | BSY | DRDY|DF/SE|  #  | DRQ |     |     | ERR |
    +-----+-----+-----+-----+-----+-----+-----+-----+
       |     |     |     |     |     |     |     |
       |     |     |     |     |     |     |     `--- 0. Error.(an error
   occurred) |     |     |     |     |     |     `--------- 1. Obsolete. |     |
   |     |     |     `--------------- 2. Obsolete. |     |     |     |
   `--------------------- 3. Data Request. (ready to transfer data) |     | |
   `--------------------------- 4. Command dependent. (formerly DSC bit) |     |
   `--------------------------------- 5. Device Fault / Stream Error. |
   `--------------------------------------- 6. Drive Ready.
       `--------------------------------------------- 7. Busy. If BSY=1, no
   other bits in the register are valid.
*/
#define STATUS_BSY 0x80
#define STATUS_DRDY 0x40
#define STATUS_DFSE 0x20
#define STATUS_DSC 0x10
#define STATUS_DRQ 0x08
#define STATUS_CORR 0x04
#define STATUS_IDX 0x02
#define STATUS_ERR 0x01

#define REG_CMD REG_STATUS /*	Command				O		*/
                           /*
                               +--------+---------------------------------+-----------------+
                               | Command| Command Description             | Parameters Used |
                               | Code   |                                 | PC SC SN CY DH  |
                               +--------+---------------------------------+-----------------+
                               | ECh  @ | Identify Drive                  |             D   |
                               | 91h    | Initialize Drive Parameters     |    V        V   |
                               | 20h    | Read Sectors With Retry         |    V  V  V  V   |
                               | E8h  @ | Write Buffer                    |             D   |
                               +--------+---------------------------------+-----------------+

                               KEY FOR SYMBOLS IN THE TABLE:
                               ===========================================-----=========================================================================
                               PC    Register 1F1: Write Precompensation	@     These commands are
                              optional and may not be supported by some drives.                            SC    Register 1F2: Sector
                              Count		D     Only DRIVE parameter is valid, HEAD parameter is ignored.
                               SN    Register 1F3: Sector Number		D+    Both drives execute this
                              command regardless of the DRIVE parameter.                            CY
                              Register 1F4+1F5: Cylinder                            low + high	V                            Indicates
                              that the register                            contains a valid paramterer.                            DH
                              Register 1F6: Drive / Head
                           */

/* Control Block Registers */
/*	MACRO		PORT			DESCRIPTION			INPUT/OUTPUT	*/
/*	-----		----			-----------			------------	*/
#define REG_DEV_CTRL 0 /*	Device Control			O		*/
                       /*	|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
                           +-----+-----+-----+-----+-----+-----+-----+-----+
                           | HOB |  -  |  -  |  -  |  -  |SRST |-IEN |  0  |
                           +-----+-----+-----+-----+-----+-----+-----+-----+
                              |                             |     |
                              |                             |     `--------- Interrupt Enable.
                              |                             |                  - IEN=0, and the drive
                          is selected,                        |                             |                        drive
                          interrupts to the host will be enabled.                        |                        |                        -                        IEN=1,
                          or the drive is not selected,                        |                        |                        drive                        interrupts to
                          the host will be disabled.                        |                        `--------------- Software
                          Reset.                        |                        - The drive is held
                          reset when RST=1.                        |                        Setting
                          RST=0 re-enables the                        drive.                        |                        -
                          The host must                        set                        RST=1 and
                          wait for at least                        |                        5
                          microsecondsbefore                        setting RST=0, to ensure                        |                        that the
                          drive recognizes                        the reset.
                              `--------------------------------------------- HOB (High Order Byte)
                                                                               - defined by 48-bit
                          Address feature set.
                       */
#define REG_ALT_STATUS REG_DEV_CTRL /*	Alternate Status		I		*/
/*	This register contains the same information as the Status Register.
    The only difference is that reading this register does not imply interrupt
   acknowledge or clear a pending interrupt.
*/

#define REG_DRV_ADDR 1 /*	Drive Address			I		*/

/* DMA registers */
#define DMA_REG_CMD 0
#define DMA_CMD_START 0x01
#define DMA_CMD_RW 0x08
#define DMA_REG_STATUS 2
#define DMA_ST_BM_ACTIVE 0x01 /* Bus Master IDE active */
#define DMA_ST_ERROR 0x02     /* Error */
#define DMA_ST_INT 0x04       /* Interrupt */
#define DMA_REG_PRDTP 4

#define MAX_IO_BYTES 256 /* how many sectors does one IO can handle */

#define MAX_DRIVES 4

/**
 * @def MAX_PRIM
 * Defines the max minor number of the primary partitions.
 * If there are 2 disks, prim_dev ranges in hd[0-9], this macro will
 * equals 9.
 */

#define MAX_PRIM (MAX_DRIVES * NR_PRIM_PER_DRIVE - 1)

#define MAX_SUBPARTITIONS (NR_SUB_PER_DRIVE * MAX_DRIVES)

/* device numbers of hard disk */
#define MINOR_hd1 0x1
#define MINOR_hd1a 0x10
#define MINOR_hd2a (MINOR_hd1a + NR_SUB_PER_PART)

struct hd_cmd {
    u8 features;
    u8 count;
    u8 lba_low;
    u8 lba_mid;
    u8 lba_high;
    u8 device;
    u8 command;
};

/* main drive struct, one entry per drive */
struct ata_info {
#define IGNORING 1
#define IDENTIFIED 2
    int state;
    int status;
    int open_cnt;
    int irq_hook;
    int native;

    u32 base_cmd;
    u32 base_ctl;
    u32 base_dma;
    u8 ldh;

    u8 dma;
    u8 dma_int;

    u32 cylinders;
    u32 heads;
    u32 sectors;

    device_id_t port_dev_id;

    struct part_info primary[NR_PRIM_PER_DRIVE];
    struct part_info logical[NR_SUB_PER_DRIVE];
};

/***************/
/* DEFINITIONS */
/***************/
#define HD_TIMEOUT 10000 /* in millisec */
#define ATA_IDENTIFY 0xEC
#define ATA_READ 0x20
#define ATA_WRITE 0x30
#define ATA_READ_DMA 0xC8  /* read data using DMA */
#define ATA_WRITE_DMA 0xCA /* write data using DMA */

/* Identify words */
#define ID_CAPABILITIES 0x31
#define ID_CAP_DMA 0x0100
#define ID_CAP_LBA 0x0200

/* for DEVICE register. */
#define MAKE_DEVICE_REG(lba, ldh, lba_highest) \
    (((lba) << 6) | (ldh) | (lba_highest & 0xF) | 0xA0)

#endif /* _HD_H_ */
