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

#include <signal.h>
#include <lyos/param.h>
    
#define SYS_CALL_MASK_SIZE  BITCHUNKS(NR_SYS_CALL)

#define PRX_OUTPUT      1
#define PRX_REGISTER    2
PUBLIC  int     printl(const char *fmt, ...);
PUBLIC  int     kernlog_register();

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

#define KF_MMINHIBIT    1
PUBLIC int      kernel_fork(endpoint_t parent_ep, int child_proc, endpoint_t * child_ep, int flags);

PUBLIC int      kernel_clear(endpoint_t ep);

#define KEXEC_ENDPOINT  u.m4.m4i1
#define KEXEC_SP        u.m4.m4p1
#define KEXEC_NAME      u.m4.m4p2
#define KEXEC_IP        u.m4.m4p3
#define KEXEC_PSSTR     u.m4.m4p4
PUBLIC int      kernel_exec(endpoint_t ep, void * sp, char * name, void * ip, struct ps_strings * ps);

PUBLIC int      kernel_sigsend(endpoint_t ep, struct siginfo * si);
PUBLIC int      kernel_kill(endpoint_t ep, int signo);

PUBLIC int      get_procep(pid_t pid, endpoint_t * ep);

PUBLIC int      get_ksig(endpoint_t * ep, sigset_t * set);
PUBLIC int      end_ksig(endpoint_t ep);

#endif
