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

#ifndef	_STACKFRAME_H_
#define	_STACKFRAME_H_

typedef unsigned long reg_t;

struct stackframe {
    reg_t sepc;
    reg_t ra;
    reg_t sp;
    reg_t kernel_sp;
    reg_t gp;
    reg_t tp;
    reg_t t0;
    reg_t t1;
    reg_t t2;
    reg_t s0;
    reg_t s1;
    reg_t a0;
    reg_t a1;
    reg_t a2;
    reg_t a3;
    reg_t a4;
    reg_t a5;
    reg_t a6;
    reg_t a7;
    reg_t s2;
    reg_t s3;
    reg_t s4;
    reg_t s5;
    reg_t s6;
    reg_t s7;
    reg_t s8;
    reg_t s9;
    reg_t s10;
    reg_t s11;
    reg_t t3;
    reg_t t4;
    reg_t t5;
    reg_t t6;
    /* Supervisor CSRs */
    reg_t sstatus;
    reg_t sbadaddr;
    reg_t scause;

    reg_t orig_a0;

    /* Current CPU */
    unsigned int cpu;
};

struct segframe {
};

struct sigcontext {
    sigset_t mask;
};

struct sigframe {
    int retaddr_sigreturn;
    int signum;
    int code;
    struct sigcontext *scp;
    int retaddr;
    struct sigcontext *scp_sigreturn;
    struct sigcontext sc;   /* actual saved context */
};

#endif
