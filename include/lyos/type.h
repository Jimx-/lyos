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

#ifndef _TYPE_H_
#define _TYPE_H_

#include <lyos/config.h>

/* routine types */
#define PUBLIC         /* PUBLIC is the opposite of PRIVATE */
#define PRIVATE static /* PRIVATE x limits the scope of x */

#define EXTERN extern

typedef unsigned long long u64;
PRIVATE inline u64 make64(unsigned long hi, unsigned long lo)
{
    return ((u64)hi << 32) | (u64)lo;
}
PRIVATE inline unsigned long ex64lo(u64 i) { return (unsigned long)i; }

PRIVATE inline unsigned long ex64hi(u64 i) { return (unsigned long)(i >> 32); }

typedef int s32;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#ifdef CONFIG_PHYS_BYTES_64BIT
typedef u64 phys_bytes;
#else
typedef u32 phys_bytes;
#endif
typedef unsigned long vir_bytes;

typedef unsigned int block_t;

typedef int endpoint_t;
typedef unsigned long priv_map_t;

typedef u32 bitchunk_t;

typedef void (*int_handler)();
typedef void (*task_f)();

typedef unsigned long irq_id_t;
typedef unsigned long irq_hook_id_t;
typedef unsigned long irq_policy_t;
typedef struct irq_hook {
    int irq;
    irq_hook_id_t id;
    int (*handler)(struct irq_hook*);
    struct irq_hook* next;
    endpoint_t proc_ep;
    irq_hook_id_t notify_id;
    irq_policy_t irq_policy;
} irq_hook_t;

typedef int (*irq_handler_t)(irq_hook_t* irq_hook);

typedef u16 port_t;

struct vir_addr {
    void* addr;
    endpoint_t proc_ep;
};

struct proc;

struct boot_proc {
    int proc_nr;
    char name[16];
    endpoint_t endpoint;
    phys_bytes base;
    phys_bytes len;
};

struct ps_strings {
    char* ps_argvstr; /* first of 0 or more argument strings */
    int ps_nargvstr;  /* the number of argument strings */
    char* ps_envstr;  /* first of 0 or more environment strings */
    int ps_nenvstr;   /* the number of environment strings */
};

struct siginfo {
    int mask;
    int signo;
    void* sig_handler;
    void* sig_return;
    void* stackptr;
};

#endif /* _TYPE_H_ */
