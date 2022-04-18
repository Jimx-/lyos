/*
    (c)Copyright 2011 Jimx

    This file is part of Lyos.

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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include <lyos/sysutils.h>
#include "global.h"
#include "proto.h"
#include "const.h"

static void pm_init();
static void process_system_notify(MESSAGE* m);

int main(int argc, char* argv[])
{
    pm_init();

    MESSAGE msg;

    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case NOTIFY_MSG:
            if (src == SYSTEM) process_system_notify(&msg);
            break;
        case FORK:
            msg.RETVAL = do_fork(&msg);
            break;
        case WAIT:
            msg.RETVAL = do_wait(&msg);
            break;
        case EXIT:
            msg.RETVAL = do_exit(&msg);
            break;
        case SIGACTION:
            msg.u.m_pm_signal.retval = do_sigaction(&msg);
            break;
        case SIGSUSPEND:
            msg.RETVAL = do_sigsuspend(&msg);
            break;
        case SIGPROCMASK:
            msg.RETVAL = do_sigprocmask(&msg);
            break;
        case KILL:
            msg.RETVAL = do_kill(&msg);
            break;
        case GETSETID:
            msg.RETVAL = do_getsetid(&msg);
            break;
        case PTRACE:
            msg.RETVAL = do_ptrace(&msg);
            break;
        case FUTEX:
            msg.RETVAL = do_futex(&msg);
            break;
        case PM_SIGRETURN:
            msg.RETVAL = do_sigreturn(&msg);
            break;
        case PM_GETPROCEP:
            msg.RETVAL = do_getprocep(&msg);
            break;
        case PM_GETINFO:
            msg.RETVAL = do_pm_getinfo(&msg);
            break;
        case PM_KPROFILE:
            msg.RETVAL = do_pm_kprofile(&msg);
            break;
        case PM_VFS_GETSETID_REPLY:
            src = msg.ENDPOINT;
            break;
        case PM_GETEPINFO:
            msg.RETVAL = do_pm_getepinfo(&msg);
            break;
        case EXEC:
            msg.RETVAL = do_exec(&msg);
            break;
        case PM_SIGNALFD_DEQUEUE:
            msg.RETVAL = do_signalfd_dequeue(&msg);
            break;
        case PM_SIGNALFD_GETNEXT:
            msg.RETVAL = do_signalfd_getnext(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        if (msg.RETVAL != SUSPEND) {
            msg.type = SYSCALL_RET;
            send_recv(SEND_NONBLOCK, src, &msg);
        }
    }

    return 0;
}

static void pm_init()
{
    int retval = 0;
    struct boot_proc boot_procs[NR_BOOT_PROCS];
    struct pmproc* pmp;
    MESSAGE vfs_msg;

    printl("PM: process manager is running.\n");

    futex_init();

    /* signal sets */
    static char core_sigs[] = {SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
                               SIGEMT,  SIGFPE, SIGBUS,  SIGSEGV};
    static char ign_sigs[] = {SIGCHLD, SIGWINCH, SIGCONT};
    static char noign_sigs[] = {SIGILL, SIGTRAP, SIGEMT,
                                SIGFPE, SIGBUS,  SIGSEGV};
    char* sig_ptr;
    sigemptyset(&core_set);
    for (sig_ptr = core_sigs; sig_ptr < core_sigs + sizeof(core_sigs);
         sig_ptr++)
        sigaddset(&core_set, *sig_ptr);
    sigemptyset(&ign_set);
    for (sig_ptr = ign_sigs; sig_ptr < ign_sigs + sizeof(ign_sigs); sig_ptr++)
        sigaddset(&ign_set, *sig_ptr);
    sigemptyset(&noign_set);
    for (sig_ptr = noign_sigs; sig_ptr < noign_sigs + sizeof(noign_sigs);
         sig_ptr++)
        sigaddset(&noign_set, *sig_ptr);

    if ((retval = get_bootprocs(boot_procs)) != 0)
        panic("cannot get boot procs from kernel");
    procs_in_use = 0;

    struct boot_proc* bp;
    for (bp = boot_procs; bp < &boot_procs[NR_BOOT_PROCS]; bp++) {
        if (bp->proc_nr < 0) continue;

        procs_in_use++;

        pmp = &pmproc_table[bp->proc_nr];
        pmp->flags = 0;
        if (bp->proc_nr == INIT) {
            pmp->parent = INIT;
            pmp->pid = pmp->procgrp = INIT_PID;
        } else {
            if (bp->proc_nr == TASK_SERVMAN)
                pmp->parent = INIT;
            else
                pmp->parent = TASK_SERVMAN;

            pmp->pid = find_free_pid();
        }

        sigemptyset(&pmp->sig_mask);
        sigemptyset(&pmp->sig_ignore);
        sigemptyset(&pmp->sig_pending);
        sigemptyset(&pmp->sig_catch);
        sigemptyset(&pmp->sig_trace);
        pmp->sig_status = 0;

        pmp->flags |= PMPF_INUSE;
        pmp->endpoint = bp->endpoint;
        pmp->tracer = NO_TASK;
        pmp->tgid = pmp->pid;
        pmp->group_leader = pmp;
        INIT_LIST_HEAD(&pmp->thread_group);

        vfs_msg.type = PM_VFS_INIT;
        vfs_msg.PROC_NR = bp->proc_nr;
        vfs_msg.PID = pmp->pid;
        vfs_msg.ENDPOINT = bp->endpoint;
        send_recv(SEND, TASK_FS, &vfs_msg);
    }

    vfs_msg.type = PM_VFS_INIT;
    vfs_msg.ENDPOINT = NO_TASK;
    send_recv(BOTH, TASK_FS, &vfs_msg);
    if (vfs_msg.RETVAL != 0) panic("pm_init: bad reply from vfs");
}

static void process_system_notify(MESSAGE* m)
{
    sigset_t sigset = m->SIGSET;

    if (sigismember(&sigset, SIGKSIG)) {
        endpoint_t target;
        sigset_t sigset;
        int retval;

        while (TRUE) {
            get_ksig(&target, &sigset);

            if (target == NO_TASK) break;

            int signo;
            for (signo = 0; signo < NSIG; signo++) {
                if (sigismember(&sigset, signo)) {
                    retval = process_ksig(target, signo);
                }
            }

            if (retval == 0) end_ksig(target);
        }
    }
}
