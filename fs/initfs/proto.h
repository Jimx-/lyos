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

#ifndef _INITFS_PROTO_H_
#define _INITFS_PROTO_H_

#include "tar.h"

PUBLIC void initfs_rw_dev(int rw_flag, int dev, int position, int length,
                          void* buf);

PUBLIC int initfs_readsuper(MESSAGE* p);

PUBLIC int initfs_lookup(MESSAGE* p);

PUBLIC unsigned int initfs_getsize(const char* in);
PUBLIC unsigned int initfs_get8(const char* in);
PUBLIC unsigned int initfs_getmode(struct posix_tar_header* phdr);

PUBLIC int initfs_stat(MESSAGE* p);

PUBLIC int initfs_rdwt(MESSAGE* p);

#endif
