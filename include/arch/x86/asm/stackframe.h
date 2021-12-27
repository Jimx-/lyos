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

#include <lyos/config.h>
#include <asm/sigcontext.h>
#include <asm/protect.h>

typedef unsigned long reg_t;

#ifdef CONFIG_X86_32 /* 32-bit: */

struct stackframe {
    reg_t gs;
    reg_t fs;
    reg_t es;
    reg_t ds;
    reg_t di;
    reg_t si;
    reg_t bp;
    reg_t kernel_sp;
    reg_t bx;
    reg_t dx;
    reg_t cx;
    reg_t ax;
    reg_t retaddr;
    reg_t ip;
    reg_t cs;
    reg_t flags;
    reg_t sp;
    reg_t ss;
    reg_t orig_ax;
};

#else /* 64-bit: */

struct stackframe {
    reg_t r15;
    reg_t r14;
    reg_t r13;
    reg_t r12;
    reg_t bp;
    reg_t bx;
    reg_t r11;
    reg_t r10;
    reg_t r9;
    reg_t r8;
    reg_t ax;
    reg_t cx;
    reg_t dx;
    reg_t si;
    reg_t di;
    reg_t orig_ax;
    reg_t ip;
    reg_t cs;
    reg_t flags;
    reg_t sp;
    reg_t ss;
    reg_t kernel_sp;
    reg_t retaddr;
};

#endif

struct segframe {
    int trap_style;
    unsigned long cr3_phys;
    unsigned long* cr3_vir;
    char* fpu_state;
    unsigned long fsbase;
    unsigned long gsbase;
    struct descriptor tls_array[GDT_TLS_ENTRIES];
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
