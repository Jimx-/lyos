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
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/sysutils.h>
#include "libfsdriver/libfsdriver.h"

PUBLIC int fsdriver_copyin(struct fsdriver_data * data, size_t offset, void * buf, size_t len)
{
    return data_copy(SELF, buf, data->src, (void *)((unsigned int)data->buf + offset), len);
}

PUBLIC int fsdriver_copyout(struct fsdriver_data * data, size_t offset, void * buf, size_t len)
{
    return data_copy(data->src, (void *)((unsigned int)data->buf + offset), SELF, buf, len);
}
