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
   
#ifndef _TRACE_H_
#define _TRACE_H_

#define TRACE_REQ       u.m3.m3i2
#define TRACE_ENDPOINT  u.m3.m3i3
#define TRACE_ADDR      u.m3.m3p1
#define TRACE_DATA      u.m3.m3p2
#define TRACE_RET       u.m3.m3l1

/* parameter for sys_trace */
#define TRACE_STOP      1
#define TRACE_SYSCALL   2
#define TRACE_CONT      3
    
#endif
