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
#include <asm/type.h>

#define SYS_CALL_MASK_SIZE BITCHUNKS(NR_SYS_CALL)

#define PRX_OUTPUT 1
#define PRX_REGISTER 2
int printl(const char* fmt, ...);
int kernlog_register();

int get_sysinfo(struct sysinfo** sysinfo);
int get_kinfo(kinfo_t* kinfo);
int get_bootprocs(struct boot_proc* bp);
int get_kernel_cmdline(char* buf, int buflen);
int get_machine(struct machine* machine);
int get_cpuinfo(struct cpu_info* cpuinfo);
int get_proctab(struct proc* proc);
int get_cputicks(unsigned int cpu, u64* ticks);
int privctl(int whom, int request, void* data);

int data_copy(endpoint_t dest_pid, void* dest_addr, endpoint_t src_pid,
              void* src_addr, int len);

/* env.c */
void env_setargs(int argc, char* argv[]);
int env_get_param(const char* key, char* value, int max_len);
int env_get_long(char* key, long* value, const char* fmt, int field, long min,
                 long max);

int syscall_entry(int syscall_nr, MESSAGE* m);

void panic(const char* fmt, ...);

#define KF_MMINHIBIT 0x1
int kernel_fork(endpoint_t parent_ep, int child_proc, endpoint_t* child_ep,
                int flags, void* newsp);

int kernel_clear(endpoint_t ep);

#define KEXEC_ENDPOINT u.m4.m4i1
#define KEXEC_SP u.m4.m4p1
#define KEXEC_NAME u.m4.m4p2
#define KEXEC_IP u.m4.m4p3
#define KEXEC_PSSTR u.m4.m4p4
int kernel_exec(endpoint_t ep, void* sp, char* name, void* ip,
                struct ps_strings* ps);

int kernel_sigsend(endpoint_t ep, struct siginfo* si);
int kernel_sigreturn(endpoint_t ep, void* scp);
int kernel_kill(endpoint_t ep, int signo);

int get_procep(pid_t pid, endpoint_t* ep);

int get_ksig(endpoint_t* ep, sigset_t* set);
int end_ksig(endpoint_t ep);

int get_system_hz();
#define BOOT_TICKS u.m3.m3l1
#define IDLE_TICKS u.m3.m3l2
int get_ticks(clock_t* ticks, clock_t* idle_ticks);

#define EXP_TIME u.m3.m3l1
#define ABS_TIME u.m3.m3i2
#define TIME_LEFT u.m3.m3l1
#define kernel_alarm(expire_time, abs_time) \
    kernel_alarm2(expire_time, abs_time, NULL)
int kernel_alarm2(clock_t expire_time, int abs_time, clock_t* time_left);

u32 now();

int kernel_trace(int request, endpoint_t endpoint, void* addr, void* data);

#define PM_INFO_PROCTAB 1
int pm_getinfo(int request, void* dest, int size);

int kernel_kprofile(int action, size_t size, int freq, endpoint_t endpt,
                    void* info, void* buf);

void* mmap_for(endpoint_t forwhom, void* addr, size_t len, int prot, int flags,
               int fd, off_t offset);
void* mm_remap(endpoint_t dest, endpoint_t src, void* dest_addr, void* src_addr,
               size_t size);

pid_t get_epinfo(endpoint_t ep, uid_t* euid, gid_t* egid);

#endif
