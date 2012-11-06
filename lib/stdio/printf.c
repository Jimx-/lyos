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
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"


/*****************************************************************************
 *                                printf
 *****************************************************************************/
/**
 * The most famous one.
 *
 * @note do not call me in any TASK, call me in USER PROC.
 * 
 * @param fmt  The format string
 * 
 * @return  The number of chars printed.
 *****************************************************************************/
PUBLIC int printf(const char *fmt, ...)
{
	int i;
	char buf[STR_DEFAULT_LEN];
	va_list arg;

	va_start(arg, fmt);
	i = vsprintf(buf, fmt, arg);
	int c = write(1, buf, i);
	assert(c == i);

	va_end(arg);

	return i;
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
PUBLIC int printl(const char *fmt, ...)
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

