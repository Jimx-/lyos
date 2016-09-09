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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/ipc.h>
#include <sys/stat.h>
#include <lyos/sysutils.h>
#include <sys/syslimits.h>
#include "libfsdriver/libfsdriver.h"

PRIVATE int get_next_name(char ** ptr, char ** start, char * name, size_t namesize)
{
	char * p;
	int i;

	for (p = *ptr; *p == '/'; p++);

	*start = p;

	if (*p) {
		for (i = 0; *p && *p != '/' && i < namesize; p++, i++)
			name[i] = *p;

		if (i >= namesize)
			return ENAMETOOLONG;

		name[i] = 0;
	} else
		strlcpy(name, ".", namesize);

	*ptr = p;
	return 0;
}

PRIVATE int access_dir(struct fsdriver_node * fn, struct vfs_ucred * ucred)
{
	mode_t mask;

	if (!S_ISDIR(fn->fn_mode)) return ENOTDIR;

	if (ucred->uid == SU_UID) return 0;

	if (ucred->uid == fn->fn_uid) mask = S_IXUSR;
	else if (ucred->gid == fn->fn_gid) mask = S_IXGRP;
	else {
		mask = S_IXOTH;
	}

	return (fn->fn_mode & mask) ? 0 : EACCES;
}

PRIVATE int resolve_link(struct fsdriver * fsd, dev_t dev, ino_t num, char * pathname, int path_len, char * ptr)
{
	struct fsdriver_data data;
	char path[PATH_MAX];
	int retval;

	data.src = SELF;
	data.buf = path;
	size_t size = sizeof(path) - 1;

	if (fsd->fs_rdlink == NULL) return ENOSYS;

	if ((retval = fsd->fs_rdlink(dev, num, &data, &size)) != 0)
		return retval;

	if (size + strlen(ptr) >= sizeof(path))
		return ENAMETOOLONG;

	strlcpy(&path[size], ptr, sizeof(path) - size);

	strlcpy(pathname, path, path_len);

	return 0;
}

PUBLIC int fsdriver_lookup(struct fsdriver * fsd, MESSAGE * m)
{
    int src = m->source;
    int dev = m->REQ_DEV;
    int start = m->REQ_START_INO;
    int root = m->REQ_ROOT_INO;
    //int flags = (int)m->REQ_FLAGS;
    int name_len = m->REQ_NAMELEN;
    //off_t offset;
    struct fsdriver_node cur_node, next_node;
    int retval = 0, is_mountpoint;
    struct vfs_ucred ucred;
    int symloop = 0;

    char pathname[PATH_MAX];
    if (name_len > PATH_MAX - 1) return ENAMETOOLONG;

    data_copy(SELF, pathname, src, m->REQ_PATHNAME, name_len);
    pathname[name_len] = '\0';

    data_copy(SELF, &ucred, src, m->REQ_UCRED, sizeof(ucred));

    if (fsd->fs_lookup == NULL) return ENOSYS;

	char * cp, * last;
	char component[NAME_MAX + 1];

	strlcpy(component, ".", sizeof(component));
	retval = fsd->fs_lookup(dev, start, component, &cur_node, &is_mountpoint);
	if (retval) return retval;

	/* scan */
	for (cp = last = pathname; *cp != '\0';) {
		if ((retval = get_next_name(&cp, &last, component, sizeof(component))) != 0) 
			break;

		if (is_mountpoint) {
			if (strcmp(component, "..")) {
				retval = EINVAL;
				break;
			}
		} else {
			if ((retval = access_dir(&cur_node, &ucred)) != 0) 
				break;
		}

		if (strcmp(component, ".") == 0) continue;

		if (strcmp(component, "..") == 0) {
			if (cur_node.fn_num == root) continue;

			if (cur_node.fn_num == fsd->root_num) {
				cp = last;
				retval = ELEAVEMOUNT;

				break;
			}
		}

		if ((retval = fsd->fs_lookup(dev, cur_node.fn_num, component, &next_node, &is_mountpoint)) != 0)
			break;

		/* resolve symlink */
		if (S_ISLNK(next_node.fn_mode) && (*cp)) {
			if (++symloop < _POSIX_SYMLOOP_MAX) {
				retval = resolve_link(fsd, dev, next_node.fn_num, pathname, sizeof(pathname), cp);
			} else
				retval = ELOOP;

			if (fsd->fs_putinode) fsd->fs_putinode(dev, next_node.fn_num);

			if (retval) break;

			cp = pathname;

			/* absolute symlink */
			if (*cp == '/') {
				retval = ESYMLINK;
				break;
			}

			continue;
		}

		if (fsd->fs_putinode) fsd->fs_putinode(dev, cur_node.fn_num);
		cur_node = next_node;

		if (is_mountpoint) {
			retval = EENTERMOUNT;
			break;
		}
	}

	if (retval == EENTERMOUNT || retval == ELEAVEMOUNT || retval == ESYMLINK) {
		int retval2;

		if (symloop > 0) {
			retval2 = data_copy(src, m->REQ_PATHNAME, SELF, pathname, strlen(pathname) + 1);
		} else retval2 = 0;

		if (retval2 == 0) {
			m->RET_OFFSET = cp - pathname;
			if (retval == EENTERMOUNT) {
				m->RET_NUM = cur_node.fn_num;
			}
		} else retval = retval2;
	}

	if (retval == 0) {
		m->RET_NUM = cur_node.fn_num;
    	m->RET_UID = cur_node.fn_uid;
    	m->RET_GID = cur_node.fn_gid;
    	m->RET_FILESIZE = cur_node.fn_size;
    	m->RET_MODE = cur_node.fn_mode;
    	m->RET_SPECDEV = cur_node.fn_device;
    } else {
    	if (fsd->fs_putinode) fsd->fs_putinode(dev, cur_node.fn_num);
    }

    return retval;
}
