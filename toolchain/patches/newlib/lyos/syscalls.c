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

#include "const.h"

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

	cmb();

	send_recv(BOTH, TASK_MM, &msg);
	//assert(msg.type == SYSCALL_RET);
}

int execv(const char *path, char * argv[])
{
	return execve(path, argv, NULL);
}

int execve(const char *name, char * argv[], char * const envp[]) 
{
	char **p = argv, **q = NULL;
	char arg_stack[PROC_ORIGIN_STACK / 2];
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

int fork()
{
	MESSAGE msg;
	msg.type = FORK;

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

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

int waitpid(int pid, int * status, int options)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = pid;
	msg.SIGNR = options;

	cmb();

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

	cmb();

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return (msg.PID == NO_TASK ? -1 : msg.PID);
}

int isatty(int fd) {
	return 0;
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
	return 0;
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

	return msg.RETVAL;
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
