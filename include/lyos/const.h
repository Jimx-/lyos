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

#ifndef _CONST_H_
#define _CONST_H_

#include <uapi/lyos/const.h>

/* return value */
#define OK 0

/* max() & min() */
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define roundup(x, align) \
    (((x) % align == 0) ? (x) : (((x) + align) - ((x) % align)))
#define rounddown(x, align) ((x) - ((x) % align))

#define TRUE  1
#define FALSE 0

#define STR_DEFAULT_LEN 1024

#define NR_IOREQS 64

#define DEFAULT_HZ 100 /* clock freq (software settable on IBM-PC) */

/* Hardware interrupts */
#define NR_IRQ 64 /* Number of IRQs */

#define ANY     (NR_PROCS + 10)
#define NO_TASK (NR_PROCS + 20)

#define MAX_TICKS 0x7FFFABCD

#define BASIC_SYSCALLS                                               \
    NR_PRINTX, NR_SENDREC, NR_GETINFO, NR_SETGRANT, NR_SAFECOPYFROM, \
        NR_SAFECOPYTO, NR_TIMES, NR_STIME

/* kernel signals */
#define SIGKSIG SIGUSR1
#define SIGKMEM SIGUSR2

#define IS_VFS_TXN_ID(t) ((t) >= VFS_TXN_BASE && (t) < MM_REQ_BASE)
#define IS_BDEV_REQ(x)   ((x) >= BDEV_OPEN && (x) <= BDEV_IOCTL)
#define IS_CDEV_REQ(x)   ((x) >= CDEV_OPEN && (x) <= CDEV_SELECT)

#define MSG_PAYLOAD u.m_payload

/* Macros for data_copy(). */
#define SRC_EP    u.m3.m3i1
#define SRC_ADDR  u.m3.m3p1
#define SRC_SEG   u.m3.m3l1
#define DEST_EP   u.m3.m3i2
#define DEST_ADDR u.m3.m3p2
#define DEST_SEG  u.m3.m3l2

/* Macros for fault. */
#define FAULT_ADDR    u.m3.m3p1
#define FAULT_PROC    u.m3.m3i3
#define FAULT_ERRCODE u.m3.m3i4
#define FAULT_PC      u.m3.m3p2

/* Hard Drive */
#define SECTOR_SIZE       512
#define SECTOR_BITS       (SECTOR_SIZE * 8)
#define SECTOR_SIZE_SHIFT 9

/* TODO: device major dynamic allocation */
#define NO_DEV       0
#define DEV_RD       1
#define DEV_FLOPPY   2
#define DEV_HD       3
#define DEV_CHAR_TTY 4
#define DEV_CHAR_PTY 5
#define DEV_INPUT    13
#define DEV_CHAR_FB  29
#define DEV_DRM      34
#define DEV_VD       112
#define DEV_NVME     114

/* make device number from major and minor numbers */
#define MAJOR_SHIFT    8
#define MINOR_MASK     ((1U << MAJOR_SHIFT) - 1)
#define MAKE_DEV(a, b) (((a) << MAJOR_SHIFT) | (b))
/* separate major and minor numbers from device number */
#define MAJOR(x) ((dev_t)(x >> MAJOR_SHIFT))
#define MINOR(x) ((dev_t)(x & MINOR_MASK))

#define MINOR_INITRD 120

#define NR_OPEN_DEFAULT 32
#define NR_FILE_DESC    256 /* FIXME */
#define NR_INODE        256 /* FIXME */
#define NR_VFS_MOUNT    16

#define NO_BLOCK ((block_t)0)

/* Flag bits for i_mode in the inode. */
#define ALL_MODES 0007777 /* all bits for user, group and others */
#define RWX_MODES 0000777 /* mode bits for RWX only */
#define R_BIT     0000004 /* Rwx protection bit */
#define W_BIT     0000002 /* rWx protection bit */
#define X_BIT     0000001 /* rwX protection bit */

/* super user uid */
#define SU_UID ((uid_t)0)

/* Char driver access bits */
#define CDEV_R_BIT  0x1
#define CDEV_W_BIT  0x2
#define CDEV_NOCTTY 0x4

/* Char driver flags */
#define CDEV_NONBLOCK 0x1

/* Char driver return flags */
#define CDEV_CLONED 0x10000000
#define CDEV_CTTY   0x20000000

/* Socket driver flags */
#define SDEV_NONBLOCK 0x01

/* Net devices */
#define ETH_PACKET_MAX 1514

#endif /* _CONST_H_ */
