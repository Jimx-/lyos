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

typedef u32 reg_t;

struct stackframe {
    reg_t r0;
    reg_t r1;
    reg_t r2;
    reg_t r3;
    reg_t r4;
    reg_t r5;
    reg_t r6;
    reg_t r7;
    reg_t r8;
    reg_t r9;  /*  sb */
    reg_t r10; /*  sl */
    reg_t r11;
    reg_t r12; /*  ip */
    reg_t sp;  /*  r13 */
    reg_t lr;  /*  r14 */
    reg_t pc;  /*  r15  */
    reg_t psr;
};

struct segframe {
    reg_t ttbr_phys;
    reg_t* ttbr_vir;
};

struct sigcontext {
    sigset_t mask;
};

struct sigframe {
    void* retaddr_sigreturn;
    int signum;
    int code;
    struct sigcontext* scp;
    int retaddr;
    struct sigcontext* scp_sigreturn;
    struct sigcontext sc; /* actual saved context */
};

#endif
