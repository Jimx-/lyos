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

PUBLIC int initfs_lookup(MESSAGE * p)
{
	int src = p->source;
	int dev = p->REQ_DEV;
	int name_len = p->REQ_NAMELEN;

	char string[TAR_MAX_PATH];

	data_copy(SELF, string, src, p->REQ_PATHNAME, name_len);
	string[name_len] = '\0';

	char * pathname = string; 
	while (*pathname == '/') {
		pathname++;
	}

	int i;
	char filename[TAR_MAX_PATH];
	for (i = 0; i < initfs_headers_count; i++) {
		initfs_rw_dev(BDEV_READ, dev, initfs_headers[i], TAR_MAX_PATH, filename);
		if (strcmp(filename, pathname) == 0) {
			char header[512];
			initfs_rw_dev(BDEV_READ, dev, initfs_headers[i], 512, header);
			struct posix_tar_header * phdr = (struct posix_tar_header*)header;
			p->RET_NUM = i;
			p->RET_UID = initfs_get8(phdr->uid);
			p->RET_GID = initfs_get8(phdr->gid);
			p->RET_FILESIZE = initfs_getsize(phdr->size);
			p->RET_MODE = initfs_getmode(phdr);
			int major = initfs_get8(phdr->devmajor);
			int minor = initfs_get8(phdr->devminor);
    		p->RET_SPECDEV = MAKE_DEV(major, minor);
    		return 0;
		}
	}

	return ENOENT;
}
