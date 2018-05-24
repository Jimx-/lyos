#include <sched.h>
#include <errno.h>
#include <../../type.h>
#include <../../ipc.h>
#include <../../const.h>

#define STACK_CHILD	0

int __clone(MESSAGE* msg);

int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg, ...)
{
	if (!child_stack) return -EINVAL;
	if (!fn) return -EINVAL;

	/* save parameters in the new stack */
	int* params = (int*) child_stack;
#define SAVE_PARAM(param) do { \
							params--; \
							*params = (int)(param); \
						} while(0)

	SAVE_PARAM(arg);
	SAVE_PARAM(fn);
	/* child's stack */
	SAVE_PARAM(STACK_CHILD);

	MESSAGE msg;
	msg.type = FORK;
	msg.FLAGS = flags;
	msg.BUF = params;

	/* construct message for sendrec() */
	/* don't use sendrec directly because we don't have return address in
	 * the new stack */
	MESSAGE msg_sendrec;
	memset(&msg_sendrec, 0, sizeof(msg_sendrec));
	msg_sendrec.type = NR_SENDREC;
	msg_sendrec.SR_FUNCTION = BOTH;
	msg_sendrec.SR_SRCDEST = TASK_PM;
	msg_sendrec.SR_MSG = &msg;

	__asm__ __volatile__ ("" ::: "memory");

	__clone(&msg_sendrec);

	if (msg.RETVAL) return -msg.RETVAL;
	return msg.PID;
}
