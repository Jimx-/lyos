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

#ifndef _STACKFRAME_H_
#define _STACKFRAME_H_

typedef unsigned long reg_t;

struct stackframe {
    reg_t regs[31];
    reg_t sp;
    reg_t pc;
    reg_t pstate;
    reg_t kernel_sp;
    reg_t orig_x0;

    reg_t stackframe[2];

    /* Current CPU */
    unsigned int cpu;
    unsigned int __unused0;
};

struct segframe {
    reg_t ttbr_phys;
    reg_t* ttbr_vir;
    char* fpu_state;
};

struct sigcontext {
    sigset_t mask;
};

struct sigframe {
    int retaddr_sigreturn;
    int signum;
    int code;
    struct sigcontext* scp;
    int retaddr;
    struct sigcontext* scp_sigreturn;
    struct sigcontext sc; /* actual saved context */
};

#endif
