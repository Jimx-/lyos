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

#include <lyos/config.h>
#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <string.h>
#include <lyos/sysutils.h>

int syscall_entry(int syscall_nr, MESSAGE* m);

PUBLIC int printx(char* s)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));
    m.REQUEST = PRX_OUTPUT;
    m.BUF = s;
    m.BUF_LEN = strlen(s);

    return syscall_entry(NR_PRINTX, &m);
}

PUBLIC int kernlog_register()
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));
    m.REQUEST = PRX_REGISTER;

    return syscall_entry(NR_PRINTX, &m);
}

/*****************************************************************************
 *                                printl
 *****************************************************************************/
/**
 * low level print
 *
 * @param fmt  The format string
 *
 * @return  The number of chars printed.
 *****************************************************************************/
PUBLIC int printl(const char* fmt, ...)
{
    int i;
    char buf[STR_DEFAULT_LEN];
    va_list arg;

    va_start(arg, fmt);
    i = vsprintf(buf, fmt, arg);
    printx(buf);

    va_end(arg);

    return i;
}
