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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/sysutils.h>
#include "libfsdriver/libfsdriver.h"

int fsdriver_copyin(struct fsdriver_data* data, size_t offset, void* buf,
                    size_t len)
{
    return data_copy(SELF, buf, data->src, (void*)(data->buf + offset), len);
}

int fsdriver_copyout(struct fsdriver_data* data, size_t offset, void* buf,
                     size_t len)
{
    return data_copy(data->src, (void*)(data->buf + offset), SELF, buf, len);
}

int fsdriver_copy_name(endpoint_t endpoint, mgrant_id_t grant, size_t len,
                       char* name, size_t size, int non_empty)
{
    int retval;

    if (non_empty && !len) return EINVAL;
    if (len > size) return ENAMETOOLONG;

    if ((retval = safecopy_from(endpoint, grant, 0, name, len)) != 0)
        return retval;
    name[len] = '\0';

    return 0;
}
