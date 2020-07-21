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

#ifndef _PM_PROTO_H_
#define _PM_PROTO_H_

pid_t find_free_pid();
int pm_verify_endpt(endpoint_t ep, int* proc_nr);
struct pmproc* pm_endpt_proc(endpoint_t ep);
struct pmproc* pm_pid_proc(pid_t pid);

int do_fork(MESSAGE* p);
int do_wait(MESSAGE* p);
int do_exit(MESSAGE* p);
int do_sigaction(MESSAGE* p);
int do_kill(MESSAGE* p);
int do_getsetid(MESSAGE* p);
int do_sigprocmask(MESSAGE* p);
int do_sigsuspend(MESSAGE* p);
int do_sigreturn(MESSAGE* p);
int do_ptrace(MESSAGE* m);
int do_exec(MESSAGE* m);
int do_futex(MESSAGE* m);
int do_pm_getinfo(MESSAGE* p);
int do_pm_kprofile(MESSAGE* msg);
int do_getprocep(MESSAGE* p);
int do_pm_getepinfo(MESSAGE* p);

int waiting_for(struct pmproc* parent, struct pmproc* child);
void exit_proc(struct pmproc* pmp, int status);

void sig_proc(struct pmproc* p_dest, int signo, int trace);
int process_ksig(endpoint_t target, int signo);

void trace_signal(struct pmproc* p_dest, int signo);

void futex_init(void);

#endif
