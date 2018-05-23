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
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/ipc.h>
#include <sys/syslimits.h>
#include <sys/dirent.h>
#include "libfsdriver/libfsdriver.h"

PUBLIC void panic(char * msg, ...);

#define _DIRENT_NAMEOFF(dp) \
     ((char *)(void *)&(dp)->d_name - (char *)(void *)dp)
#define _DIRENT_RECLEN(dp, namlen) \
     ((_DIRENT_NAMEOFF(dp) + (namlen) + 1 + 0xf) & ~0xf)

PUBLIC int fsdriver_dentry_list_init(struct fsdriver_dentry_list * list, struct fsdriver_data * data, 
                        size_t data_size, char * buf, size_t buf_size)
{
	list->data = data;
	list->data_size = data_size;
	list->data_offset = 0;
	list->buf = buf;
	list->buf_size = buf_size;
	list->buf_offset = 0;

	return 0;
}

PUBLIC int fsdriver_dentry_list_add(struct fsdriver_dentry_list * list , ino_t num, char * name,
                size_t name_len, int type)
{
	int retval = 0;

	if (name_len > NAME_MAX) panic("fsdriver: name too long!\n");

	struct dirent * dp;
	int len = _DIRENT_RECLEN(dp, name_len);

	if (list->data_offset + list->buf_offset + len > list->data_size) {
		return 0;
	}

	if (list->buf_offset + len > list->buf_size) {
		if (list->buf_offset == 0)
			panic("fsdriver: getdents buffer too small");

		if ((retval = fsdriver_copyout(list->data, list->data_offset,
		    list->buf, list->buf_offset)) != 0)
			return retval;

		list->data_offset += list->buf_offset;
		list->buf_offset = 0;
	}

	dp = (struct dirent *)((char *)list->buf + list->buf_offset);
	dp->d_ino = num;
	dp->d_reclen = len;
	dp->d_type = type;
	memcpy(dp->d_name, name, name_len);
	dp->d_name[name_len] = '\0';

	list->buf_offset += len;

	return len;
}

PUBLIC int fsdriver_dentry_list_finish(struct fsdriver_dentry_list * list)
{
	int retval;

	if (list->buf_offset > 0) {
		if ((retval = fsdriver_copyout(list->data, list->data_offset,
		    list->buf, list->buf_offset)) != 0)
			return -retval;

		list->data_offset += list->buf_offset;
	}

	return list->data_offset;
}

