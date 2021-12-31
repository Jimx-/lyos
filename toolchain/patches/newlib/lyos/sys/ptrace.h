#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

#include <sys/cdefs.h>

#define PTRACE_TRACEME 0
#define PT_TRACE_ME    PTRACE_TRACEME

#define PTRACE_PEEKTEXT           1
#define PTRACE_PEEKDATA           2
#define PTRACE_PEEKUSER           3
#define PTRACE_POKETEXT           4
#define PTRACE_POKEDATA           5
#define PTRACE_CONT               7
#define PTRACE_KILL               8
#define PTRACE_SINGLESTEP         9
#define PTRACE_GETREGS            14
#define PTRACE_SETREGS            15
#define PTRACE_ATTACH             16
#define PTRACE_DETACH             17
#define PTRACE_GETFPXREGS         18
#define PTRACE_SETFPXREGS         19
#define PTRACE_SYSCALL            24
#define PTRACE_SETOPTIONS         0x4200
#define PTRACE_GETEVENTMSG        0x4201
#define PTRACE_GETSIGINFO         0x4202
#define PTRACE_SETSIGINFO         0x4203
#define PTRACE_GETREGSET          0x4204
#define PTRACE_SETREGSET          0x4205
#define PTRACE_SEIZE              0x4206
#define PTRACE_INTERRUPT          0x4207
#define PTRACE_LISTEN             0x4208
#define PTRACE_PEEKSIGINFO        0x4209
#define PTRACE_GETSIGMASK         0x420A
#define PTRACE_SETSIGMASK         0x420B
#define PTRACE_SECCOMP_GET_FILTER 0x420C

#define PTRACE_REQ  u.m3.m3i2
#define PTRACE_PID  u.m3.m3i3
#define PTRACE_ADDR u.m3.m3p1
#define PTRACE_DATA u.m3.m3p2
#define PTRACE_RET  u.m3.m3l1

__BEGIN_DECLS

long ptrace(int request, ...);

__END_DECLS

#endif
