#include "type.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <assert.h>

#include "const.h"
#include "proc.h"

/* compiler memory barrier */
#define cmb() __asm__ __volatile__ ("" ::: "memory")

/* sendrec */
/* see arch/x86/lib/syscall.asm */
static int sendrec(int function, int src_dest, MESSAGE* msg)
{
	int a;
	__asm__ __volatile__("int $0x90" : "=a" (a) : "0" (1), "b" (function), "c" (src_dest), "d" ((int)msg));
	return a;
}

int send_recv(int function, int src_dest, MESSAGE* msg)
{
	int ret = 0;

	if (function == RECEIVE)
		memset(msg, 0, sizeof(MESSAGE));

	switch (function) {
	case BOTH:
		ret = sendrec(SEND, src_dest, msg);
		if (ret == 0)
			ret = sendrec(RECEIVE, src_dest, msg);
		break;
	case SEND:
	case RECEIVE:
		ret = sendrec(function, src_dest, msg);
		break;
	default:
		//assert((function == BOTH) ||
		//       (function == SEND) || (function == RECEIVE));
		break;
	}

	return ret;
}


extern char **environ;

void _exit(int status)
{
	MESSAGE msg;
	msg.type	= EXIT;
	msg.STATUS	= status;

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);
}

int execv(const char *path, char * argv[])
{
	char **p = argv;
	char arg_stack[PROC_ORIGIN_STACK];
	int stack_len = 0;

	while(*p++) {
		//assert(stack_len + 2 * sizeof(char*) < PROC_ORIGIN_STACK);
		stack_len += sizeof(char*);
	}

	*((int*)(&arg_stack[stack_len])) = 0;
	stack_len += sizeof(char*);

	char ** q = (char**)arg_stack;
	for (p = argv; *p != 0; p++) {
		*q++ = &arg_stack[stack_len];

		strcpy(&arg_stack[stack_len], *p);
		stack_len += strlen(*p);
		arg_stack[stack_len] = 0;
		stack_len++;
	}

	MESSAGE msg;
	msg.type	= EXEC;
	msg.PATHNAME	= (void*)path;
	msg.NAME_LEN	= strlen(path);
	msg.BUF		= (void*)arg_stack;
	msg.BUF_LEN	= stack_len;

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

int execve(const char *name, char * argv[], char * const envp[]) 
{
	return execv(name, argv);
}

int execvp(const char *file, char * argv[])
{
	return execv(file, argv);
}

int getpid()
{
	MESSAGE msg;
	msg.type	= GET_PID;
    
    cmb();

	send_recv(BOTH, TASK_SYS, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.PID;
}

int fork()
{
	MESSAGE msg;
	msg.type = FORK;

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);
	//assert(msg.RETVAL == 0);

	return msg.PID;
}

int kill(int pid,int signo)
{
	MESSAGE msg;
	msg.type	= KILL;
	msg.PID		= pid;
	msg.SIGNR	= signo;

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

int waitpid(int pid, int * status, int options)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = pid;
	msg.SIGNR = options;

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return (msg.PID == NO_TASK ? -1 : msg.PID);
}

int wait(int * status)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = -1;
	msg.SIGNR = 0;

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return (msg.PID == NO_TASK ? -1 : msg.PID);
}

int isatty(int fd) {
	return 0;
}

int uname(struct utsname * name)
{
    return 0;
}

int close(int fd)
{
	MESSAGE msg;
	msg.type   = CLOSE;
	msg.FD     = fd;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int link(char *old, char *new)
{
	return 0;
}

int unlink(const char * pathname)
{
	MESSAGE msg;
	msg.type   = UNLINK;

	msg.PATHNAME	= (void*)pathname;
	msg.NAME_LEN	= strlen(pathname);

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int fcntl(int fd, int cmd, ...)
{
	return 0;
}

mode_t umask(mode_t mask )
{
	return 0;
}

int chmod(const char *path, mode_t mode)
{
	return 0;
}

int access(const char *pathname,int mode)
{
	return 0;
}

int lseek(int fd, int offset, int whence)
{
	MESSAGE msg;
	msg.type   = LSEEK;
	msg.FD     = fd;
	msg.OFFSET = offset;
	msg.WHENCE = whence;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.OFFSET;
}

int open(const char *pathname, int flags, ...)
{
	MESSAGE msg;

	msg.type	= OPEN;

	msg.PATHNAME	= (void*)pathname;
	msg.FLAGS	= flags;
	msg.NAME_LEN	= strlen(pathname);

	cmb();
	
	send_recv(BOTH, TASK_FS, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.FD;
}

int read(int fd, void *buf, int count)
{
	MESSAGE msg;
	msg.type = READ;
	msg.FD   = fd;
	msg.BUF  = buf;
	msg.CNT  = count;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.CNT;
}

int creat(const char *path, mode_t mode) {
	return open(path, O_WRONLY|O_CREAT|O_TRUNC);
}

int stat(const char *path, struct stat *buf)
{
	MESSAGE msg;

	msg.type	= STAT;

	msg.PATHNAME	= (void*)path;
	msg.BUF		= (void*)buf;
	msg.NAME_LEN	= strlen(path);

	send_recv(BOTH, TASK_FS, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

int rmdir(const char *path )
{
	return 0;
}	

int fstat(int fd, struct stat *buf)
{
	MESSAGE msg;

	msg.type	= FSTAT;

	msg.FD		= fd;
	msg.BUF		= (void*)buf;

	send_recv(BOTH, TASK_FS, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

int lstat(const char *path, struct stat *buf)
{
	return stat(path, buf);
}

int write(int fd, const void *buf, int count)
{
	MESSAGE msg;
	msg.type = WRITE;
	msg.FD   = fd;
	msg.BUF  = (void*)buf;
	msg.CNT  = count;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.CNT;
}

char *getwd(char *buf)
{
	return (char*)0;
}

long pathconf(const char *path, int name)
{
	return 0;
}

int utime(const char *filename, const struct utimbuf *times)
{
	return 0;
}

int chown(const char *path, uid_t owner, gid_t group)
{
	return 0;
}

caddr_t sbrk(int nbytes){
	return (caddr_t)0;
}

int gettimeofday(struct timeval* tv, void *tz)
{
	MESSAGE msg;

	msg.type = GET_TIME_OF_DAY;
	msg.BUF  = (void*)tv;

	cmb();

	send_recv(BOTH, TASK_SYS, &msg);
	
	return msg.RETVAL;
}

