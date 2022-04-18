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
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include <stdarg.h>
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"

static char* buf;
static size_t used, left;
static off_t offset;

#define BUF_SIZE 4096

void init_buf(char* ptr, size_t len, off_t off)
{
    buf = ptr;
    offset = off;
    left = min(len, BUF_SIZE);
    used = 0;
}

void buf_printf(char* fmt, ...)
{
    va_list args;
    ssize_t len, max;

    if (left == 0) return;

    max = min(offset + left + 1, BUF_SIZE);

    va_start(args, fmt);
    len = vsnprintf(&buf[used], max, fmt, args);
    va_end(args);

    /*
     * The snprintf family returns the number of bytes that would be stored
     * if the buffer were large enough, excluding the null terminator.
     */
    if (len >= BUF_SIZE) len = BUF_SIZE - 1;

    if (offset > 0) {

        if (offset >= len) {
            offset -= len;

            return;
        }

        memmove(buf, &buf[offset], len - offset);

        len -= offset;
        offset = 0;
    }

    if (len > (ssize_t)left) len = left;

    used += len;
    left -= len;
}

size_t buf_used() { return used; }
