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

#include <lyos/ipc.h>
#include "lyos/list.h"
#include <lyos/const.h>
#include <lyos/fs.h>
#include "types.h"
#include "global.h"

DEF_LIST(vfs_mount_table);
DEF_LIST(filesystem_table);

int have_root = 0;
