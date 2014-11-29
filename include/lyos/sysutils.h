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

#ifndef _SYSUTILS_H_
#define _SYSUTILS_H_

#include <lyos/param.h>
    
PUBLIC  int     printl(const char *fmt, ...);
PUBLIC  int     get_sysinfo(struct sysinfo ** sysinfo);
PUBLIC  int     get_kinfo(kinfo_t * kinfo);
PUBLIC  int     get_bootprocs(struct boot_proc * bp);
PUBLIC  int     data_copy(endpoint_t dest_pid, void * dest_addr, 
    endpoint_t src_pid, void * src_addr, int len);

/* env.c */
PUBLIC void     env_setargs(int argc, char * argv[]);
PUBLIC int      env_get_param(const char * key, char * value, int max_len);
PUBLIC int      env_get_long(char * key, long * value, const char * fmt, int field, long min, long max);

PUBLIC int      syscall_entry(int syscall_nr, MESSAGE * m);

PUBLIC void     panic(const char *fmt, ...);

#endif
