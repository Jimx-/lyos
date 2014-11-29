#include "type.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/termios.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/dirent.h>
#include <sys/resource.h>

#include "const.h"

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE * m);
int syscall_gate_intr(int syscall_nr, MESSAGE * m);

extern syscall_gate_t _syscall_gate;

/* compiler memory barrier */
#define cmb() __asm__ __volatile__ ("" ::: "memory")

int syscall_entry(int syscall_nr, MESSAGE * m)
{
	if (_syscall_gate) return _syscall_gate(syscall_nr, m);
	return syscall_gate_intr(syscall_nr, m);
}

/* sendrec */
/* see arch/x86/lib/syscall.asm */
static int sendrec(int function, int src_dest, MESSAGE* msg)
{
	MESSAGE m;
	memset(&m, 0, sizeof(m));
	m.SR_FUNCTION = function;
	m.SR_SRCDEST = src_dest;
	m.SR_MSG = msg;

	return syscall_entry(NR_SENDREC, &m);
}

int send_recv(int function, int src_dest, MESSAGE* msg)
{
	int ret = 0;

	if (function == RECEIVE)
		memset(msg, 0, sizeof(MESSAGE));

	return sendrec(function, src_dest, msg);
}

extern char **environ;

void _exit(int status)
{
	MESSAGE msg;
	msg.type	= EXIT;
	msg.STATUS	= status;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);
}

int execv(const char *path, char * argv[])
{
	return execve(path, argv, NULL);
}

#define ARG_MAX	0x40000

int execve(const char *name, char * argv[], char * const envp[]) 
{
	char **p = argv, **q = NULL;
	char arg_stack[ARG_MAX];
	int stack_len = 0;

	/* arg_stack layout */
	/* | argv | 0 | envp | 0 | strings | */
	if (p) {
		while(*p++) {
			stack_len += sizeof(char*);
		}
	}

	*((int*)(&arg_stack[stack_len])) = 0;
	stack_len += sizeof(char*);

	int env_start = stack_len / sizeof(char*);

	p = envp;
	if (p) {
		while (*p++) {
			stack_len += sizeof(char*);
		}
	}

	*((int*)(&arg_stack[stack_len])) = 0;
	stack_len += sizeof(char*);

	if (argv) {
		q = (char**)arg_stack;
		for (p = argv; *p != 0; p++) {
			*q++ = &arg_stack[stack_len];

			strcpy(&arg_stack[stack_len], *p);
			stack_len += strlen(*p);
			arg_stack[stack_len] = 0;
			stack_len++;
		}
	}

	if (envp) {
		q = (char**)arg_stack + env_start;
		for (p = envp; *p != 0; p++) {
			*q++ = &arg_stack[stack_len];

			strcpy(&arg_stack[stack_len], *p);
			stack_len += strlen(*p);
			arg_stack[stack_len] = 0;
			stack_len++;
		}
	}

	MESSAGE msg;
	msg.type	= EXEC;
	msg.PATHNAME	= (void*)name;
	msg.NAME_LEN	= strlen(name);
	msg.BUF		= (void*)arg_stack;
	msg.BUF_LEN	= stack_len;
	msg.TARGET = getpid();

	cmb();

	send_recv(BOTH, TASK_FS, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
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

pid_t getppid(void)
{
	return INIT;
}

pid_t getpgid(pid_t pid)
{
	return INIT;
}

int setpgid(pid_t pid, pid_t pgid)
{
	return 0;
}

pid_t getpgrp(void)
{
	return INIT;
}

int fork()
{
	MESSAGE msg;
	msg.type = FORK;

	cmb();

	send_recv(BOTH, TASK_PM, &msg);

	return msg.PID;
}

int kill(int pid,int signo)
{
	MESSAGE msg;
	msg.type	= KILL;
	msg.PID		= pid;
	msg.SIGNR	= signo;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	MESSAGE msg;

	msg.type = SIGACTION;
	msg.NEWSA = act;
	msg.OLDSA = oldact;
	msg.SIGNR = signum;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

sighandler_t signal(int signum, sighandler_t handler)
{
	return handler;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	return 0;
}

int sigsuspend(const sigset_t *mask)
{
	return 0;
}

int waitpid(int pid, int * status, int options)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = pid;
	msg.SIGNR = options;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return msg.PID;
}

pid_t wait3(int *status, int options, struct rusage *rusage)
{
	return waitpid(-1, status, options);
}

int wait(int * status)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = -1;
	msg.SIGNR = 0;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return msg.PID;
}

int getgroups(int size, gid_t list[])
{
	return 0;
}

int isatty(int fd) {
	struct termios t;

	return (tcgetattr(fd, &t) != -1);
}

int uname(struct utsname * name)
{
	MESSAGE msg;
	msg.type = UNAME;
	msg.BUF = (void *)name;

	cmb();

	send_recv(BOTH, TASK_SYS, &msg);

    return msg.RETVAL;
}

int close(int fd)
{
	MESSAGE msg;
	msg.type   = CLOSE;
	msg.FD     = fd;

	cmb();
	
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

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int fcntl(int fd, int cmd, ...)
{
	MESSAGE msg;
	msg.type   = FCNTL;

	msg.FD	= fd;
	msg.REQUEST	= cmd;

	va_list argp;

 	va_start(argp, cmd);

	msg.BUF_LEN = 0;

	switch(cmd) {
	case F_DUPFD:
	case F_SETFD:
	case F_SETFL:
		msg.BUF_LEN = va_arg(argp, int);
		break;
  	}

	va_end(argp);

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	if (msg.RETVAL < 0) errno = -msg.RETVAL;
	return msg.RETVAL < 0 ? -1 : msg.RETVAL;
}

mode_t umask(mode_t mask)
{
	MESSAGE msg;
	msg.type = UMASK;

	msg.MODE = mask;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int dup(int fd)
{
	return dup2(fd, -1);
}

int dup2(int fd, int fd2)
{
	MESSAGE msg;
	msg.type = DUP;

	msg.FD = fd;
	msg.NEWFD = fd2;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int chdir(const char * path)
{
	MESSAGE msg;
	msg.type = CHDIR;

	msg.PATHNAME = path;
	msg.NAME_LEN = strlen(path);

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int fchdir(int fd)
{
	MESSAGE msg;
	msg.type = FCHDIR;

	msg.FD = fd;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int chmod(const char *path, mode_t mode)
{
	return 0;
}

int mount(const char *source, const char *target,
          const char *filesystemtype, unsigned long mountflags,
          const void *data)
{
	MESSAGE msg;
	msg.type = MOUNT;

	msg.MFLAGS = mountflags;
	msg.MNAMELEN1 = strlen(source);
	msg.MNAMELEN2 = strlen(target);
	msg.MNAMELEN3 = strlen(filesystemtype);
	msg.MSOURCE = source;
	msg.MTARGET = target;
	msg.MLABEL = filesystemtype;
	msg.MDATA = data;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int access(const char *pathname, int mode)
{
	MESSAGE msg;
	msg.type   = ACCESS;
	msg.PATHNAME = pathname;
	msg.NAME_LEN = strlen(pathname);
	msg.MODE = mode;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int lseek(int fd, int offset, int whence)
{
	MESSAGE msg;
	msg.type   = LSEEK;
	msg.FD     = fd;
	msg.OFFSET = offset;
	msg.WHENCE = whence;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	/* on error */
	if (msg.RETVAL != 0) {
		errno = msg.RETVAL;
		return -1;
	}

	return msg.OFFSET;
}

int open(const char *pathname, int flags, ...)
{
	MESSAGE msg;
	va_list parg;

	msg.type	= OPEN;
	
	msg.PATHNAME	= (void*)pathname;
	msg.FLAGS	= flags;
	msg.NAME_LEN	= strlen(pathname);

	va_start(parg, flags);
	msg.MODE = va_arg(parg, mode_t);
	va_end(parg);
	
	cmb();
	
	send_recv(BOTH, TASK_FS, &msg);
	//assert(msg.type == SYSCALL_RET);
	
	if (msg.FD < 0) {
		errno = -msg.FD;
		return -1;
	}

	return msg.FD;
}

int read(int fd, void *buf, int count)
{
	MESSAGE msg;
	msg.type = READ;
	msg.FD   = fd;
	msg.BUF  = buf;
	msg.CNT  = count;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	return msg.CNT;
}

int ioctl(int fd, int request, void * data)
{
	MESSAGE msg;
	msg.type = IOCTL;
	msg.FD = fd;
	msg.REQUEST = request;
	msg.BUF = data;

	cmb();

	send_recv(BOTH, TASK_FS, &msg);

	if (msg.RETVAL != 0) errno = msg.RETVAL;
	return msg.RETVAL == 0 ? 0 : -1;
}

int creat(const char *path, mode_t mode) {
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int stat(const char *path, struct stat *buf)
{
	MESSAGE msg;

	msg.type	= STAT;

	msg.PATHNAME	= (void*)path;
	msg.BUF		= (void*)buf;
	msg.NAME_LEN	= strlen(path);

	cmb();

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

	cmb();

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

	cmb();
	
	send_recv(BOTH, TASK_FS, &msg);

	return msg.CNT;
}

DIR * opendir (const char * dirname)
{
	return NULL;
}

int closedir (DIR * dir)
{
	return 0;
}

struct dirent * readdir (DIR * dirp)
{
	return NULL;
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

caddr_t sbrk(int nbytes)
{
	MESSAGE msg;
	msg.type = SBRK;
	msg.CNT  = nbytes;

	cmb();
	
	send_recv(BOTH, TASK_MM, &msg);

	return (caddr_t)msg.RETVAL;
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

int getuid()
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_GETUID;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int setuid(uid_t uid)
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_SETUID;
	msg.NEWID = uid;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int getgid()
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_GETGID;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int setgid(gid_t gid)
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_SETGID;
	msg.NEWID = gid;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int geteuid()
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_GETEUID;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int getegid()
{
	MESSAGE msg;

	msg.type = GETSETID;
	msg.REQUEST = GS_GETEGID;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	return msg.RETVAL;
}

int gethostname(char *name, size_t len)
{
	MESSAGE msg;

	msg.type = GETSETHOSTNAME;
	msg.REQUEST = GS_GETHOSTNAME;
	msg.BUF = name;
	msg.BUF_LEN = len;

	cmb();

	send_recv(BOTH, TASK_SYS, &msg);

	if (msg.RETVAL) {
		errno = msg.RETVAL;
		return -1;
	}

	return 0;
}

int sethostname(const char *name, size_t len)
{
	MESSAGE msg;

	msg.type = GETSETHOSTNAME;
	msg.REQUEST = GS_SETHOSTNAME;
	msg.BUF = name;
	msg.BUF_LEN = len;

	cmb();

	send_recv(BOTH, TASK_SYS, &msg);

	if (msg.RETVAL) {
		errno = msg.RETVAL;
		return -1;
	}

	return 0;
}

int tcgetattr(int fd, struct termios * tio) {
	return ioctl(fd, TCGETS, tio);
}

int tcsetattr(int fd, int actions, struct termios * tio) {
	switch (actions) {
		case TCSANOW:
			return ioctl(fd, TCSETS, tio);
		case TCSADRAIN:
			return ioctl(fd, TCSETSW, tio);
		case TCSAFLUSH:
			return ioctl(fd, TCSETSF, tio);
		default:
			return 0;
	}
}

int tcsetpgrp(int fd, pid_t pgrp)
{
	return 0;
}

pid_t tcgetpgrp(int fd)
{
	return 0;
}

char *getcwd(char *buf, size_t size)
{
	if (buf == NULL) buf = (char *)malloc(size);
	if (buf == NULL) return NULL;
	strcpy(buf, "/home");
	return buf;
}

int pipe(int pipefd[2])
{

}
