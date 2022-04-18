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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/ipc.h>
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include <unistd.h>

int printl(const char* fmt, ...);

void __panic(const char* fmt, ...)
{
    char buf[STR_DEFAULT_LEN];
    va_list arg;

    printl("panic: ");
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    printl(buf);
    va_end(arg);

    _exit(1);

    for (;;)
        ;
}

void panic(const char* fmt, ...) __attribute__((weak, alias("__panic")));
