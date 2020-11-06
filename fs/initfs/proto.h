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

#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/ipc.h>

#include <libfsdriver/libfsdriver.h>

#include "tar.h"

int initfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                     struct fsdriver_node* node);
int initfs_lookup(dev_t dev, ino_t start, const char* name,
                  struct fsdriver_node* fn, int* is_mountpoint);
ssize_t initfs_read(dev_t dev, ino_t num, struct fsdriver_data* data,
                    loff_t rwpos, size_t count);
ssize_t initfs_write(dev_t dev, ino_t num, struct fsdriver_data* data,
                     loff_t rwpos, size_t count);
ssize_t initfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                        loff_t* ppos, size_t count);
int initfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data);

unsigned int initfs_getsize(const char* in);
unsigned int initfs_get8(const char* in);
unsigned int initfs_getmode(const struct posix_tar_header* phdr);
int initfs_read_header(dev_t dev, ino_t num, char* header, size_t header_size);

#endif
