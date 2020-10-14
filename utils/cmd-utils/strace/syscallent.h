#ifndef _STRACE_SYSCALLENT_H
#define _STRACE_SYSCALLENT_H

struct syscallent {
    int sys_no;
    int (*sys_func)();
    const char* sys_name;
};

#endif
