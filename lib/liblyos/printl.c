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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <string.h>

PUBLIC	int	printx(MESSAGE * m);

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
PUBLIC int printl(const char *fmt, ...)
{
	int i;
	char buf[STR_DEFAULT_LEN];
	va_list arg;
	MESSAGE m;
    memset(&m, 0, sizeof(m));

	va_start(arg, fmt);	
	i = vsprintf(buf, fmt, arg);
    
    m.BUF = buf;
	printx(&m);

	va_end(arg);

	return i;
}

