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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include <kernel/proc.h>
#include "string.h"
#include <kernel/global.h>
#include <kernel/proto.h>
#include "arch.h"

void disp_char(const char c) { machine_desc->serial_putc(c); }

void direct_put_str(const char* str)
{
    while (*str) {
        disp_char(*str);
        str++;
    }
}

int direct_print(const char* fmt, ...)
{
    int i;
    char buf[256];
    va_list arg;

    va_start(arg, fmt);
    i = vsprintf(buf, fmt, arg);
    direct_put_str(buf);

    va_end(arg);

    return i;
}

void direct_cls() {}
