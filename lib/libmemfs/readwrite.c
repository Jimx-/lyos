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
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include "libmemfs/libmemfs.h"

#define BUFSIZE		1024

PUBLIC char* memfs_buf;
PUBLIC size_t memfs_bufsize;

PUBLIC int memfs_init_buf()
{
	memfs_buf = (char*)malloc(BUFSIZE);
	if (memfs_buf == NULL) return ENOMEM;

	memfs_bufsize = BUFSIZE;
	return 0;
}

PUBLIC int memfs_free_buf()
{
	free(memfs_buf);
	memfs_buf = NULL;
	memfs_bufsize = 0;

	return 0;
}

PUBLIC int memfs_readwrite(dev_t dev, ino_t num, int rw_flag, struct fsdriver_data * data, u64 * rwpos, int * count)
{
	struct memfs_inode * pin = memfs_find_inode(num);
	if (!pin) return ENOENT;

	if (!S_ISREG(pin->i_stat.st_mode)) return EINVAL;

	if (rw_flag == READ && fs_hooks.read_hook == NULL) {
		*count = 0;
		return 0;
	}
	if (rw_flag == WRITE && fs_hooks.write_hook == NULL) {
		*count = 0;
		return 0;
	}

	off_t off;
	size_t chunk, len;
	int retval = 0;
	for (off = 0; off < *count; ) {
		chunk = *count - off;
		if (chunk > memfs_bufsize) chunk = memfs_bufsize;

		if (rw_flag == WRITE) {
			retval = fsdriver_copyin(data, off, memfs_buf, len);
		}

		if (rw_flag == READ) {
			len = fs_hooks.read_hook(pin, memfs_buf, chunk, *rwpos, pin->data);
		} else {
			len = fs_hooks.write_hook(pin, memfs_buf, chunk, *rwpos, pin->data);
		}

		if (len > 0) {
			if (rw_flag == READ)
				retval = fsdriver_copyout(data, off, memfs_buf, len);
		} else {
			retval = -len;
		}

		if (retval) {
			if (off > 0) {
				*count = off;
				return 0;
			} else return retval;
		}

		off += len;
		*rwpos += len;

		if (len < memfs_bufsize) break;
	}

	*count = off;
	return 0;
}
