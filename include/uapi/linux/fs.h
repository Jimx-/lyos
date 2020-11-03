/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_FS_H
#define _UAPI_LINUX_FS_H

#include <lyos/ioctl.h>

#define BLKROSET   _IO(0x12, 93)  /* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12, 94)  /* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12, 95)  /* re-read partition table */
#define BLKGETSIZE _IO(0x12, 96)  /* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12, 97)  /* flush buffer cache */
#define BLKRASET   _IO(0x12, 98)  /* set read ahead for block device */
#define BLKRAGET   _IO(0x12, 99)  /* get current read ahead setting */
#define BLKFRASET  _IO(0x12, 100) /* set filesystem (mm/filemap.c) read-ahead \
                                   */
#define BLKFRAGET _IO(0x12, 101)  /* get filesystem (mm/filemap.c) read-ahead \
                                   */
#define BLKSECTSET \
    _IO(0x12, 102) /* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET \
    _IO(0x12, 103)               /* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET _IO(0x12, 104) /* get block device sector size */
#if 0
#define BLKPG _IO(0x12, 105) /* See blkpg.h */

/* Some people are morons.  Do not use sizeof! */

#define BLKELVGET _IOR(0x12, 106, size_t) /* elevator get */
#define BLKELVSET _IOW(0x12, 107, size_t) /* elevator set */
/* This was here just to show that the number is taken -
   probably all these _IO(0x12,*) ioctls should be moved to blkpg.h. */
#endif
/* A jump here: 108-111 have been used for various private purposes. */
#define BLKBSZGET _IOR(0x12, 112, size_t)
#define BLKBSZSET _IOW(0x12, 113, size_t)
#define BLKGETSIZE64 \
    _IOR(0x12, 114, size_t) /* return device size in bytes (u64 *arg) */
#define BLKTRACESETUP    _IOWR(0x12, 115, struct blk_user_trace_setup)
#define BLKTRACESTART    _IO(0x12, 116)
#define BLKTRACESTOP     _IO(0x12, 117)
#define BLKTRACETEARDOWN _IO(0x12, 118)
#define BLKDISCARD       _IO(0x12, 119)
#define BLKIOMIN         _IO(0x12, 120)
#define BLKIOOPT         _IO(0x12, 121)
#define BLKALIGNOFF      _IO(0x12, 122)
#define BLKPBSZGET       _IO(0x12, 123)
#define BLKDISCARDZEROES _IO(0x12, 124)
#define BLKSECDISCARD    _IO(0x12, 125)
#define BLKROTATIONAL    _IO(0x12, 126)
#define BLKZEROOUT       _IO(0x12, 127)

#endif
