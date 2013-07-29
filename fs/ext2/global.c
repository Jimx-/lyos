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

#define _GLOBAL_VARIABLE_HERE_

#include "lyos/type.h"
#include "lyos/list.h"
#include "global.h"

PUBLIC DEF_LIST(ext2_superblock_table);
PUBLIC u8* ext2fsbuf = (u8*)&_ext2fsbuf;
