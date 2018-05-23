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

#include <lyos/compile.h>
#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <sys/mman.h>
#include <sys/shm.h>

#include "const.h"
#include "proto.h"

PUBLIC int check_perm(struct ipc_perm* perm, endpoint_t source, mode_t mode)
{
	uid_t uid;
	gid_t gid;
	mode_t perm_mode;
	get_epinfo(source, &uid, &gid);

	if (uid == perm->uid || uid == perm->cuid) {
		perm_mode = perm->mode & 0x0700;
 	} else if (gid == perm->gid || gid == perm->cgid) {
 		perm_mode = perm->mode & 0x0070;
 		mode >>= 3;
 	} else {
 		perm_mode = perm->mode & 0x0007;
 		mode >>= 6;
 	}

 	return mode && (mode & perm_mode == mode);
}
