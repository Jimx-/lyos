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

#define _VFS_GLOBAL_VARIABLE_HERE_

#include "lyos/type.h"
#include <sys/types.h>
#include "lyos/list.h"
#include <lyos/fs.h>
#include <lyos/const.h>
#include "types.h"
#include "global.h"

PUBLIC DEF_LIST(vfs_mount_table);
PUBLIC DEF_LIST(filesystem_table);

PUBLIC int have_root = 0;

PUBLIC int nr_workers = 0; 

PUBLIC  u8 * fsbuf = (u8*)&_fsbuf;

/*****************************************************************************/
/**
 * For dd_map[k],
 * `k' is the device nr.\ dd_map[k].driver_nr is the driver nr.
 *
 * Remeber to modify include/const.h if the order is changed.
 *****************************************************************************/
struct dev_drv_map dd_map[] = {
    /* driver nr.       major device nr.
       ----------       ---------------- */
    {INVALID_DRIVER},   /**< 0 : Unused */
    {TASK_RD},          /**< 1 : Ramdisk */
    {INVALID_DRIVER},   /**< 2 : Floppy */
    {INVALID_DRIVER},   /**< 3 : Hard disk */
    {TASK_TTY},         /**< 4 : TTY */
    {INVALID_DRIVER}    /**< 5 : Scsi disk */
};
