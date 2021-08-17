#include <sched.h>
#include <errno.h>
#include <stdarg.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

#define STACK_CHILD 0

int __clone(MESSAGE* msg);

int clone(int (*fn)(void* arg), void* child_stack, int flags, void* arg, ...)
{
    va_list argp;

    if (!child_stack) return -EINVAL;
    if (!fn) return -EINVAL;

    /* save parameters in the new stack */
    int* params = (int*)child_stack;
#define SAVE_PARAM(param)       \
    do {                        \
        params--;               \
        *params = (int)(param); \
    } while (0)

    SAVE_PARAM(arg);
    SAVE_PARAM(fn);
    /* child's stack */
    SAVE_PARAM(STACK_CHILD);

    MESSAGE msg;
    msg.type = FORK;
    msg.u.m_pm_clone.flags = flags;
    msg.u.m_pm_clone.stack = params;

    if (flags & (CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID |
                 CLONE_CHILD_SETTID)) {
        va_start(argp, arg);

        msg.u.m_pm_clone.parent_tid = va_arg(argp, void*);
        msg.u.m_pm_clone.tls = va_arg(argp, void*);
        msg.u.m_pm_clone.child_tid = va_arg(argp, void*);

        va_end(argp);
    }

    /* construct message for sendrec() */
    /* don't use sendrec directly because we don't have return address in
     * the new stack */
    MESSAGE msg_sendrec;
    memset(&msg_sendrec, 0, sizeof(msg_sendrec));
    msg_sendrec.type = NR_SENDREC;
    msg_sendrec.SR_FUNCTION = BOTH;
    msg_sendrec.SR_SRCDEST = TASK_PM;
    msg_sendrec.SR_MSG = &msg;

    __asm__ __volatile__("" ::: "memory");

    __clone(&msg_sendrec);

    if (msg.RETVAL) return -msg.RETVAL;
    return msg.PID;
}
