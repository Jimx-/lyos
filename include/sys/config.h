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

/**
 * Some sector are reserved for us (the gods of the os) to copy a tar file
 * there, which will be extracted and used by the OS.
 *
 * @attention INSTALL_NR_SECTS should be a multiple of NR_DEFAULT_FILE_SECTS:
 *                INSTALL_NR_SECTS = n * NR_DEFAULT_FILE_SECTS (n=1,2,3,...)
 */
#define	INSTALL_START_SECT		0x8000
#define	INSTALL_NR_SECTS		0x800

/**
 * boot parameters are stored by the loader, they should be
 * there when kernel is running and should not be overwritten
 * since kernel might use them at any time.
 */
#define	BOOT_PARAM_ADDR			0x900  /* physical address */
#define	BOOT_PARAM_MAGIC		0xB007 /* magic number */
#define	BI_MAG				0
#define	BI_MEM_SIZE			1
#define	BI_KERNEL_FILE			2

/**
 * corresponding with boot/include/load.inc::ROOT_BASE, which should
 * be changed if this macro is changed.
 */
#define	MINOR_BOOT			MINOR_hd2a

#define RAMDISK
#define FLOPPY

/*
 * disk log
 */
#define ENABLE_DISK_LOG
#define SET_LOG_SECT_SMAP_AT_STARTUP
#define MEMSET_LOG_SECTS
#define	NR_SECTS_FOR_LOG		NR_DEFAULT_FILE_SECTS
