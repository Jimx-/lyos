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

#ifndef _SERVMAN_TYPE_H_
#define _SERVMAN_TYPE_H_

#include <lyos/priv.h>
#include <lyos/proc.h>

struct sproc {
	endpoint_t endpoint;
	struct priv priv;	/* privilege structure */
};

struct boot_priv {
	endpoint_t endpoint;
	char name[PROC_NAME_LEN];
	int flags;
};

#endif
