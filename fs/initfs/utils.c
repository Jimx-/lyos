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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "proto.h"
#include "global.h"
#include "tar.h"

PUBLIC unsigned int initfs_getsize(const char* in)
{

    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

PUBLIC unsigned int initfs_get8(const char* in)
{

    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 7; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

PUBLIC unsigned int initfs_getmode(struct posix_tar_header* phdr)
{
    unsigned int mode;

    switch (phdr->typeflag) {
    case '0':
    case '\0':
        mode = I_REGULAR;
        break;
    case '3':
        mode = I_CHAR_SPECIAL;
        break;
    case '4':
        mode = I_BLOCK_SPECIAL;
        break;
    default:
        mode = 0;
        break;
    }

    mode |= initfs_get8(phdr->mode);

    return mode;
}
