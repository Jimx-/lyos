#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

#define PTRACE_TRACEME	1
#define PTRACE_SYSCALL	2
#define PTRACE_CONT		3
#define PTRACE_PEEKTEXT	4
#define PTRACE_PEEKDATA	5
#define PTRACE_PEEKUSER 6

#define PTRACE_REQ	u.m3.m3i2
#define PTRACE_PID	u.m3.m3i3
#define PTRACE_ADDR	u.m3.m3p1
#define PTRACE_DATA	u.m3.m3p2
#define PTRACE_RET	u.m3.m3i3

long ptrace(int request, pid_t pid, void* addr, void* data);

#endif
