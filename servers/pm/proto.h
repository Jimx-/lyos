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

PUBLIC pid_t find_free_pid();
PUBLIC int pm_verify_endpt(endpoint_t ep, int * proc_nr);
PUBLIC struct pmproc * pm_endpt_proc(endpoint_t ep);
PUBLIC struct pmproc* pm_pid_proc(pid_t pid);

PUBLIC int do_fork(MESSAGE * p);
PUBLIC int do_wait(MESSAGE * p);
PUBLIC int do_exit(MESSAGE * p);
PUBLIC int do_sigaction(MESSAGE * p);
PUBLIC int do_kill(MESSAGE * p);
PUBLIC int do_getsetid(MESSAGE * p);
PUBLIC int do_sigprocmask(MESSAGE * p);
PUBLIC int do_sigsuspend(MESSAGE * p);
PUBLIC int do_ptrace(MESSAGE * m);
PUBLIC int do_exec(MESSAGE * m);
PUBLIC int do_pm_getinfo(MESSAGE * p);
PUBLIC int do_pm_kprofile(MESSAGE* msg);
PUBLIC int do_pm_getepinfo(MESSAGE* p);

PUBLIC int waiting_for(struct pmproc* parent, struct pmproc * child);
PUBLIC void exit_proc(struct pmproc* pmp, int status);

PUBLIC int process_ksig(endpoint_t target, int signo);

PUBLIC void trace_signal(struct pmproc* p_dest, int signo);

#endif
