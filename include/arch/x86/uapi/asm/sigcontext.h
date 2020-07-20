#ifndef _UAPI_ASM_X86_SIGCONTEXT_H_
#define _UAPI_ASM_X86_SIGCONTEXT_H_

#include <lyos/types.h>

#ifdef __i386__
struct sigcontext {
    __u32 gs;
    __u32 fs;
    __u32 es;
    __u32 ds;
    __u32 edi;
    __u32 esi;
    __u32 ebp;
    __u32 ebx;
    __u32 edx;
    __u32 ecx;
    __u32 eax;
    __u32 eip;
    __u32 cs;
    __u32 eflags;
    __u32 esp;
    __u32 ss;

    __u32 onstack;
    __u32 __mask13;

    __u32 trapno;
    __u32 err;

    __u64 mask;
    __u32 trap_style;
};
#endif

#endif
