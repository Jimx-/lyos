#define _GNU_SOURCE

#include <lyos/type.h>
#include <lyos/ipc.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <utime.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/termios.h>
#include <sys/syslimits.h>
#include <sys/mman.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/dirent.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <sys/statfs.h>
#include <sys/futex.h>
#include <sys/shm.h>
#include <grp.h>

#include <lyos/const.h>

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE* m);
int syscall_gate_intr(int syscall_nr, MESSAGE* m);

extern syscall_gate_t _syscall_gate;

extern char* _brksize;

/* compiler memory barrier */
#define cmb() __asm__ __volatile__("" ::: "memory")

int syscall_entry(int syscall_nr, MESSAGE* m)
{
    m->type = syscall_nr;
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

    if (function == RECEIVE) memset(msg, 0, sizeof(MESSAGE));

    return sendrec(function, src_dest, msg);
}

extern char** environ;

void _exit(int status)
{
    MESSAGE msg;
    msg.type = EXIT;
    msg.STATUS = status;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);
    while (1)
        ;
}

int execve(const char* name, char* argv[], char* const envp[])
{
    char **p = argv, **q = NULL;
    char arg_stack[ARG_MAX];
    int stack_len = 0;

    /* arg_stack layout */
    /* | argv | 0 | envp | 0 | strings | */
    if (p) {
        while (*p++) {
            stack_len += sizeof(char*);
        }
    }

    *((int*)(&arg_stack[stack_len])) = 0;
    stack_len += sizeof(char*);

    int env_start = stack_len / sizeof(char*);

    p = (char**)envp;
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
        for (p = (char**)envp; *p != 0; p++) {
            *q++ = &arg_stack[stack_len];

            strcpy(&arg_stack[stack_len], *p);
            stack_len += strlen(*p);
            arg_stack[stack_len] = 0;
            stack_len++;
        }
    }

    MESSAGE msg;
    msg.type = EXEC;
    msg.PATHNAME = (void*)name;
    msg.NAME_LEN = strlen(name);
    msg.BUF = (void*)arg_stack;
    msg.BUF_LEN = stack_len;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.RETVAL;
}

int execv(const char* path, char* argv[])
{
    return execve(path, argv, environ);
}

int execvp(const char* file, char* argv[]) { return execv(file, argv); }

int execlp(const char* file, const char* arg, ...)
{
    puts("execlp: not implemented");
    return ENOSYS;
}

int execl(const char* path, const char* arg, ...)
{
    puts("execl: not implemented");
    return ENOSYS;
}

endpoint_t get_endpoint()
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETEP;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.ENDPOINT;
}

int getpid()
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETPID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.PID;
}

int gettid()
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETTID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.PID;
}

pid_t getppid(void)
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETPPID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.PID;
}

pid_t getpgid(pid_t pid)
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETPGID;
    msg.PID = pid;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.PID;
}

pid_t setsid(void)
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_SETSID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.PID;
}

int setpgid(pid_t pid, pid_t pgid)
{
    pid_t _pid, _pgid, cpid;

    _pid = pid;
    _pgid = pgid;

    /* Who are we? */
    cpid = getpid();

    /* if zero, means current process. */
    if (_pid == 0) {
        _pid = cpid;
    }

    /* if zero, means given pid. */
    if (_pgid == 0) {
        _pgid = _pid;
    }

    /* right now we only support the equivalent of setsid(), which is
     * setpgid(0,0) */
    if ((_pid != cpid) || (_pgid != cpid)) {
        errno = EINVAL;
        return -1;
    }

    if (setsid() == cpid) {
        return 0;
    } else {
        return -1;
    }
}

pid_t getpgrp(void)
{
    MESSAGE msg;
    msg.type = GETSETID;
    msg.REQUEST = GS_GETPGRP;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.PID;
}

int fork()
{
    MESSAGE msg;
    msg.type = FORK;
    msg.FLAGS = 0;
    msg.BUF = NULL;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.PID;
}

int kill(int pid, int signo)
{
    MESSAGE msg;
    msg.type = KILL;
    msg.PID = pid;
    msg.SIGNR = signo;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.RETVAL;
}

int killpg(int pgrp, int signo) { return kill(-pgrp, signo); }

void __sigreturn();

void sigreturn(void* scp)
{
    MESSAGE msg;

    msg.type = PM_SIGRETURN;
    msg.BUF = scp;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
    MESSAGE msg;

    msg.type = SIGACTION;
    msg.u.m_pm_signal.act = (void*)act;
    msg.u.m_pm_signal.oldact = oldact;
    msg.u.m_pm_signal.signum = signum;
    msg.u.m_pm_signal.sigret = __sigreturn;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.u.m_pm_signal.retval;
}

sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction sa, osa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(signum, &sa, &osa) < 0) return (SIG_ERR);

    return osa.sa_handler;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset)
{
    MESSAGE msg;
    msg.type = SIGPROCMASK;

    if (set)
        msg.MASK = *set;
    else
        set = 0;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    if (oldset) *oldset = msg.MASK;

    return 0;
}

int sigsuspend(const sigset_t* mask)
{
    MESSAGE msg;
    msg.type = SIGSUSPEND;

    msg.MASK = *mask;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int waitpid(int pid, int* status, int options)
{
    MESSAGE msg;
    msg.type = WAIT;

    msg.PID = pid;
    msg.OPTIONS = options;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (status) *status = msg.STATUS;

    return msg.PID;
}

pid_t wait3(int* status, int options, struct rusage* rusage)
{
    return waitpid(-1, status, options);
}

int wait(int* status)
{
    MESSAGE msg;
    msg.type = WAIT;

    msg.PID = -1;
    msg.OPTIONS = 0;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (status) *status = msg.STATUS;

    return msg.PID;
}

int getgroups(int size, gid_t list[])
{
    puts("getgroups: not implemented");
    return 0;
}

int isatty(int fd)
{
    struct termios t;

    return (tcgetattr(fd, &t) != -1);
}

int uname(struct utsname* name)
{
    MESSAGE msg;
    msg.type = UNAME;
    msg.BUF = (void*)name;

    cmb();

    send_recv(BOTH, TASK_SYS, &msg);

    return msg.RETVAL;
}

int close(int fd)
{
    MESSAGE msg;
    msg.type = CLOSE;
    msg.FD = fd;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int mkdir(const char* pathname, mode_t mode)
{
    MESSAGE msg;
    memset(&msg, 0, sizeof(MESSAGE));
    msg.type = MKDIR;
    msg.PATHNAME = (void*)pathname;
    msg.NAME_LEN = strlen(pathname);
    msg.MODE = mode;
    cmb();

    send_recv(BOTH, TASK_FS, &msg);
    return msg.RETVAL;
}

int mknod(const char* pathname, mode_t mode, dev_t dev)
{
    puts("mknod: not implemented");
    return ENOSYS;
}

int link(char* old, char* new)
{
    puts("link: not implemented");
    return 0;
}

int unlink(const char* pathname)
{
    MESSAGE msg;
    msg.type = UNLINK;

    msg.PATHNAME = (void*)pathname;
    msg.NAME_LEN = strlen(pathname);

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int fcntl(int fd, int cmd, ...)
{
    MESSAGE msg;
    msg.type = FCNTL;

    msg.FD = fd;
    msg.REQUEST = cmd;

    va_list argp;

    va_start(argp, cmd);

    msg.BUF_LEN = 0;

    switch (cmd) {
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

int dup(int fd) { return dup2(fd, -1); }

int chdir(const char* path)
{
    MESSAGE msg;
    msg.type = CHDIR;

    msg.PATHNAME = (char*)path;
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

int chmod(const char* path, mode_t mode)
{
    MESSAGE msg;
    msg.type = CHMOD;

    msg.PATHNAME = (char*)path;
    msg.NAME_LEN = strlen(path);
    msg.MODE = mode;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int fchmod(int fd, mode_t mode)
{
    MESSAGE msg;
    msg.type = FCHMOD;

    msg.FD = fd;
    msg.MODE = mode;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data)
{
    MESSAGE msg;
    msg.type = MOUNT;

    msg.MFLAGS = mountflags;
    if (source == NULL)
        msg.MNAMELEN1 = 0;
    else
        msg.MNAMELEN1 = strlen(source);
    msg.MNAMELEN2 = strlen(target);
    msg.MNAMELEN3 = strlen(filesystemtype);
    msg.MSOURCE = (char*)source;
    msg.MTARGET = (char*)target;
    msg.MLABEL = (char*)filesystemtype;
    msg.MDATA = (char*)data;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int access(const char* pathname, int mode)
{
    MESSAGE msg;
    msg.type = ACCESS;
    msg.PATHNAME = (char*)pathname;
    msg.NAME_LEN = strlen(pathname);
    msg.MODE = mode;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

int lseek(int fd, int offset, int whence)
{
    MESSAGE msg;
    msg.type = LSEEK;
    msg.FD = fd;
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

int open(const char* pathname, int flags, ...)
{
    MESSAGE msg;
    va_list parg;

    memset(&msg, 0, sizeof(MESSAGE));
    va_start(parg, flags);

    msg.type = OPEN;
    msg.PATHNAME = (void*)pathname;
    msg.FLAGS = flags;
    msg.NAME_LEN = strlen(pathname);

    if (flags & O_CREAT) {
        msg.MODE = va_arg(parg, mode_t);
    }

    va_end(parg);
    cmb();

    send_recv(BOTH, TASK_FS, &msg);
    // assert(msg.type == SYSCALL_RET);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}

int read(int fd, void* buf, int count)
{
    MESSAGE msg;
    msg.type = READ;
    msg.FD = fd;
    msg.BUF = buf;
    msg.CNT = count;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }
    return msg.RETVAL;
}

int ioctl(int fd, int request, void* data)
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

int creat(const char* path, mode_t mode)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int stat(const char* path, struct stat* buf)
{
    MESSAGE msg;

    msg.type = STAT;

    msg.PATHNAME = (void*)path;
    msg.BUF = (void*)buf;
    msg.NAME_LEN = strlen(path);

    cmb();

    send_recv(BOTH, TASK_FS, &msg);
    // assert(msg.type == SYSCALL_RET);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

int statfs(const char* path, struct statfs* buf)
{
    puts("statfs: not implemented");
    return ENOSYS;
}

int fstatfs(int fd, struct statfs* buf)
{
    puts("fstatfs: not implemented");
    return ENOSYS;
}

int rmdir(const char* path)
{
    puts("rmdir: not implemented");
    return 0;
}

int fstat(int fd, struct stat* buf)
{
    MESSAGE msg;

    msg.type = FSTAT;

    msg.FD = fd;
    msg.BUF = (void*)buf;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);
    // assert(msg.type == SYSCALL_RET);

    return msg.RETVAL;
}

int _stat(const char* path, mode_t mode) __attribute__((weak, alias("stat")));
int _fstat(int fd, struct stat* buf) __attribute__((weak, alias("fstat")));

int lstat(const char* path, struct stat* buf) { return stat(path, buf); }

int write(int fd, const void* buf, int count)
{
    MESSAGE msg;
    msg.type = WRITE;
    msg.FD = fd;
    msg.BUF = (void*)buf;
    msg.CNT = count;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }
    return msg.RETVAL;
}

int getdents(unsigned int fd, struct dirent* dirp, unsigned int count)
{
    MESSAGE msg;
    msg.type = GETDENTS;
    msg.FD = fd;
    msg.BUF = (void*)dirp;
    msg.CNT = count;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    return msg.RETVAL;
}

#define DIRBLKSIZ 1024
DIR* opendir(const char* name)
{
    int fd = open(name, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat sbuf;
    if (fstat(fd, &sbuf)) {
        errno = ENOTDIR;
        close(fd);
        return NULL;
    }
    if (!S_ISDIR(sbuf.st_mode)) {
        errno = ENOTDIR;
        close(fd);
        return NULL;
    }

    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) {
        close(fd);
        return NULL;
    }

    dir->len = DIRBLKSIZ;
    dir->buf = malloc(dir->len);
    if (!dir->buf) {
        close(fd);
        return NULL;
    }
    dir->loc = 0;
    dir->fd = fd;

    return dir;
}

struct dirent* readdir(DIR* dirp)
{
    struct dirent* dp;

    if (!dirp) return NULL;

    if (dirp->loc >= dirp->size) dirp->loc = 0;
    if (dirp->loc == 0) {
        if ((dirp->size = getdents(dirp->fd, dirp->buf, dirp->len)) <= 0)
            return NULL;
    }

    dp = (struct dirent*)((char*)dirp->buf + dirp->loc);
    if (dp->d_reclen >= dirp->len - dirp->loc + 1) return NULL;
    dirp->loc += dp->d_reclen;

    return dp;
}

int closedir(DIR* dirp)
{
    int fd = dirp->fd;

    free(dirp->buf);
    free(dirp);

    return close(fd);
}

extern char* getcwd(char*, size_t);
char* getwd(char* buf)
{
    if (getcwd(buf, PATH_MAX) != 0) return NULL;
    return buf;
}

long pathconf(const char* path, int name)
{
    printf("pathconf: not implemented\n");
    return 0;
}

int utime(const char* filename, const struct utimbuf* times)
{
    printf("utime: not implemented\n");
    return 0;
}

int chown(const char* path, uid_t owner, gid_t group)
{
    printf("chown: not implemented\n");
    return 0;
}

int brk(void* addr)
{
    MESSAGE msg;
    msg.type = BRK;
    msg.ADDR = addr;

    cmb();

    send_recv(BOTH, TASK_MM, &msg);

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    _brksize = addr;

    return 0;
}

caddr_t sbrk(int nbytes)
{
    char *oldsize = _brksize, *newsize = _brksize + nbytes;

    if ((nbytes < 0 && newsize > oldsize) || (nbytes > 0 && newsize < oldsize))
        return (caddr_t)(-1);
    if (brk(newsize) == 0)
        return (caddr_t)oldsize;
    else
        return (caddr_t)(-1);
}

int gettimeofday(struct timeval* tv, void* tz)
{
    MESSAGE msg;

    msg.type = GET_TIME_OF_DAY;
    msg.BUF = (void*)tv;

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

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int setuid(uid_t uid)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_SETUID;
    msg.NEWID = uid;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int getgid()
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETGID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int setgid(gid_t gid)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_SETGID;
    msg.NEWID = gid;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int setreuid(uid_t ruid, uid_t euid) { return setuid(ruid); }

int setregid(uid_t rgid, uid_t egid) { return setgid(rgid); }

int geteuid()
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETEUID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int getegid()
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETEGID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int gethostname(char* name, size_t len)
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

int sethostname(const char* name, size_t len)
{
    MESSAGE msg;

    msg.type = GETSETHOSTNAME;
    msg.REQUEST = GS_SETHOSTNAME;
    msg.BUF = (char*)name;
    msg.BUF_LEN = len;

    cmb();

    send_recv(BOTH, TASK_SYS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

/* termios */
speed_t cfgetispeed(const struct termios* tio) { return tio->c_ispeed; }

speed_t cfgetospeed(const struct termios* tio) { return tio->c_ospeed; }

int cfsetispeed(struct termios* tio, speed_t speed)
{
    tio->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios* tio, speed_t speed)
{
    tio->c_ospeed = speed;
    return 0;
}

int tcgetattr(int fd, struct termios* tio) { return ioctl(fd, TCGETS, tio); }

int tcsetattr(int fd, int actions, struct termios* tio)
{
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
    pid_t s = pgrp;
    return ioctl(fd, TIOCSPGRP, &s);
}

pid_t tcgetpgrp(int fd)
{
    pid_t pgrp;

    int retval = ioctl(fd, TIOCGPGRP, &pgrp);

    if (retval == 0)
        return pgrp;
    else {
        errno = retval;
    }

    return -1;
}

int tcflow(int fd, int action)
{
    puts("tcflow: not implemented");
    return 0;
}

int tcflush(int fd, int which)
{
    int selector = which;
    return ioctl(fd, TCFLSH, &selector);
}

int pipe(int pipefd[2]) { printf("pipe: not implemented\n"); }

int __dirfd(DIR* dirp) { return dirp->fd; }

void sync(void) { puts("sync: not implemented"); }

int fsync(int fd)
{
    puts("fsync: not implemented");
    return 0;
}

unsigned int alarm(unsigned int seconds)
{
    puts("alarm: not implemented");
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    puts("sleep: not implemented");
    return 0;
}

clock_t times(struct tms* buf)
{
    puts("times: not implemented");
    return 0;
}

int getrusage(int who, struct rusage* usage)
{
    puts("getrusage: not implemented");
    return 0;
}

struct group* getgrent(void)
{
    puts("getgrent: not implemented");
    return NULL;
}

void setgrent(void) { puts("setgrent: not implemented"); }

void endgrent(void) { puts("endgrent: not implemented"); }

long ptrace(int request, pid_t pid, void* addr, void* data)
{
    MESSAGE msg;

    msg.type = PTRACE;
    msg.PTRACE_REQ = request;
    msg.PTRACE_PID = pid;
    msg.PTRACE_ADDR = addr;
    msg.PTRACE_DATA = data;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return msg.PTRACE_RET;
}

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = MMAP;
    m.u.m_mm_mmap.who = SELF;
    m.u.m_mm_mmap.vaddr = addr;
    m.u.m_mm_mmap.length = len;
    m.u.m_mm_mmap.prot = prot;
    m.u.m_mm_mmap.flags = flags;
    m.u.m_mm_mmap.fd = fd;
    m.u.m_mm_mmap.offset = offset;

    cmb();

    send_recv(BOTH, TASK_MM, &m);

    if (m.u.m_mm_mmap_reply.retval)
        return MAP_FAILED;
    else
        return m.u.m_mm_mmap_reply.retaddr;
}

int munmap(void* addr, size_t len)
{
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = MUNMAP;
    m.u.m_mm_mmap.who = SELF;
    m.u.m_mm_mmap.vaddr = addr;
    m.u.m_mm_mmap.length = len;

    cmb();

    send_recv(BOTH, TASK_MM, &m);

    if (m.RETVAL) {
        errno = -m.RETVAL;
        return -1;
    }

    return 0;
}

struct group* getgrnam(const char* name)
{
    puts("getgrnam: not implemented");
    return NULL;
}

struct group* getgrgid(gid_t gid)
{
    puts("getgrgid: not implemented");
    return NULL;
}

int ftruncate(int fd, off_t length)
{
    puts("ftruncate: not implemented");
    return -ENOSYS;
}

char* getlogin()
{
    puts("getlogin: not implemented");
    return NULL;
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = SELECT;
    m.u.m_vfs_select.nfds = nfds;
    m.u.m_vfs_select.readfds = readfds;
    m.u.m_vfs_select.writefds = writefds;
    m.u.m_vfs_select.exceptfds = exceptfds;
    m.u.m_vfs_select.timeout = timeout;

    cmb();
    send_recv(BOTH, TASK_FS, &m);

    return m.RETVAL;
}

int futex(int* uaddr, int futex_op, int val,
          const struct timespec* timeout, /* or: uint32_t val2 */
          int* uaddr2, int val3)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = FUTEX;
    m.FUTEX_UADDR = uaddr;
    m.FUTEX_OP = futex_op;
    m.FUTEX_VAL = val;
    m.FUTEX_VAL2 = (u32)timeout;
    m.FUTEX_TIMEOUT = (u32)timeout;
    m.FUTEX_UADDR2 = uaddr2;
    m.FUTEX_VAL3 = val3;

    cmb();

    send_recv(BOTH, TASK_MM, &m);

    return m.RETVAL;
}

int kprof(int action, size_t size, int freq, void* info, void* buf)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = PM_KPROFILE;
    m.KP_ACTION = action;
    m.KP_SIZE = size;
    m.KP_FREQ = freq;
    m.KP_CTL = info;
    m.KP_BUF = buf;
    cmb();

    send_recv(BOTH, TASK_PM, &m);

    return m.RETVAL;
}

int shmget(key_t key, size_t size, int flags)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = IPC_SHMGET;
    m.IPC_KEY = key;
    m.IPC_SIZE = size;
    m.IPC_FLAGS = flags;
    cmb();

    send_recv(BOTH, TASK_IPC, &m);

    if (m.RETVAL != 0) return -m.RETVAL;
    return m.IPC_RETID;
}

void* shmat(int shmid, const void* shmaddr, int shmflg)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = IPC_SHMAT;
    m.IPC_ID = shmid;
    m.IPC_ADDR = (void*)shmaddr;
    m.IPC_FLAGS = shmflg;
    cmb();

    send_recv(BOTH, TASK_IPC, &m);

    return m.IPC_RETADDR;
}

int getentropy(void* buffer, size_t length)
{
    printf("getentropy: not implemented\n");
    return ENOSYS;
}

int posix_memalign(void** memptr, size_t alignment, size_t size) { return 0; }

int getrlimit(int resource, struct rlimit* rlim)
{
    rlim_t limit;

    switch (resource) {
    case RLIMIT_CPU:
    case RLIMIT_FSIZE:
    case RLIMIT_DATA:
    case RLIMIT_STACK:
    case RLIMIT_CORE:
    case RLIMIT_RSS:
    case RLIMIT_MEMLOCK:
    case RLIMIT_SBSIZE:
    case RLIMIT_AS:
    case RLIMIT_NTHR:
        limit = RLIM_INFINITY;
        break;

    case RLIMIT_NOFILE:
        limit = OPEN_MAX;
        break;

    default:
        errno = EINVAL;
        return -1;
    }

    rlim->rlim_cur = limit;
    rlim->rlim_max = limit;

    return 0;
}

int setrlimit(int resource, const struct rlimit* rlim)
{
    switch (resource) {
    case RLIMIT_CPU:
    case RLIMIT_FSIZE:
    case RLIMIT_DATA:
    case RLIMIT_STACK:
    case RLIMIT_CORE:
    case RLIMIT_RSS:
    case RLIMIT_MEMLOCK:
    case RLIMIT_NPROC:
    case RLIMIT_NOFILE:
    case RLIMIT_SBSIZE:
    case RLIMIT_AS:
    case RLIMIT_NTHR:
        break;

    default:
        errno = EINVAL;
        return -1;
    }

    return 0;
}
