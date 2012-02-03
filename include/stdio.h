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

#ifndef	_STDIO_H_
#define	_STDIO_H_

#include "lyos/type.h"

/* the assert macro */
#define NDEBUG /* enable/disable assert */

/* EXTERN */
#define	EXTERN	extern	/* EXTERN is defined as extern except in global.c */

/* string */
#define	STR_DEFAULT_LEN	1024

#define SEEK_SET	1
#define SEEK_CUR	2
#define SEEK_END	3

#define	MAX_PATH	128

#define  BCD_TO_DEC(x)      ( (x >> 4) * 10 + (x & 0x0f) )

/*========================*
 * printf, printl, printx *
 *========================*
 *
 *   printf:
 *
 *           [send msg]                WRITE           DEV_WRITE
 *                      USER_PROC ------------â†?FS -------------â†?TTY
 *                              â†–______________â†™â†–_______________/
 *           [recv msg]             SYSCALL_RET       SYSCALL_RET
 *
 *----------------------------------------------------------------------
 *
 *   printl: variant-parameter-version printx
 *
 *          calls vsprintf, then printx (trap into kernel directly)
 *
 *----------------------------------------------------------------------
 *
 *   printx: low level print without using IPC
 *
 *                       trap directly
 *           USER_PROC -- -- -- -- -- --> KERNEL
 *
 *
 *----------------------------------------------------------------------
 */

/* printf.c */
PUBLIC  int     printf(const char *fmt, ...);
PUBLIC  int     printl(const char *fmt, ...);

/* vsprintf.c */
PUBLIC  int     vsprintf(char *buf, const char *fmt, char* args);
PUBLIC	int	sprintf(char *buf, const char *fmt, ...);

/*--------*/
/* åº“å‡½æ•?*/
/*--------*/

#ifdef ENABLE_DISK_LOG
#define SYSLOG syslog
#endif

#endif /* _STDIO_H_ */
