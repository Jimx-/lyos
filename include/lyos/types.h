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

#ifndef _LYOS_TYPES_H_
#define _LYOS_TYPES_H_

#include <lyos/config.h>
#include <uapi/lyos/types.h>
#include <stddef.h>

#define EXTERN extern

typedef __s64 s64;
typedef __u64 u64;
typedef __s32 s32;
typedef __u32 u32;
typedef __u16 u16;
typedef __s8 s8;
typedef __u8 u8;

typedef __endpoint_t endpoint_t;

static __attribute__((always_inline)) inline u64 make64(unsigned long hi,
                                                        unsigned long lo)
{
    return ((u64)hi << 32) | (u64)lo;
}

static __attribute__((always_inline)) inline unsigned long ex64lo(u64 i)
{
    return (unsigned long)i;
}

static __attribute__((always_inline)) inline unsigned long ex64hi(u64 i)
{
    return (unsigned long)(i >> 32);
}

#ifdef CONFIG_PHYS_BYTES_64BIT
typedef u64 phys_bytes;
#else
typedef u32 phys_bytes;
#endif
typedef unsigned long vir_bytes;

typedef unsigned int block_t;

typedef unsigned long priv_map_t;

typedef unsigned long bitchunk_t;

typedef u64 loff_t;

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

typedef __mgrant_id_t mgrant_id_t;

struct vir_addr {
    void* addr;
    endpoint_t proc_ep;
};

struct umap_phys {
    phys_bytes phys_addr;
    size_t size;
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
    unsigned long mask;
    int signo;
    void* sig_handler;
    void* sig_return;
    void* stackptr;
};

struct iovec_grant {
    union {
        vir_bytes iov_addr;
        mgrant_id_t iov_grant;
    };
    size_t iov_len;
};

struct kfork_info {
    endpoint_t parent;
    int slot;
    int flags;
    void* newsp;
    void* tls;
    endpoint_t child;
};

#endif /* _TYPE_H_ */
