#define _GNU_SOURCE

#include <lyos/types.h>
#include <lyos/ipc.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
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
#include <asm/sigcontext.h>

#include <lyos/const.h>

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE* m);
int syscall_gate_intr(int syscall_nr, MESSAGE* m);

extern syscall_gate_t _syscall_gate;

extern char* _brksize;

int daylight;
long timezone;

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

int execve(const char* name, char* const argv[], char* const envp[])
{
    char **p = (char**)argv, **q = NULL;
    char arg_stack[ARG_MAX];
    int stack_len = 0;

    /* arg_stack layout */
    /* | argv | 0 | envp | 0 | strings | */
    if (p) {
        while (*p++) {
            stack_len += sizeof(char*);
        }
    }

    *((char**)(&arg_stack[stack_len])) = NULL;
    stack_len += sizeof(char*);

    int env_start = stack_len / sizeof(char*);

    p = (char**)envp;
    if (p) {
        while (*p++) {
            stack_len += sizeof(char*);
        }
    }

    *((char**)(&arg_stack[stack_len])) = NULL;
    stack_len += sizeof(char*);

    if (argv) {
        q = (char**)arg_stack;
        for (p = (char**)argv; *p != 0; p++) {
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

int execv(const char* path, char* const argv[])
{
    return execve(path, argv, environ);
}

int execvp(const char* file, char* const argv[]) { return execv(file, argv); }

int execlp(const char* file, const char* arg, ...)
{
    puts("execlp: not implemented");
    return ENOSYS;
}

int execl(const char* path, const char* arg, ...)
{
    char *argv[16], *argn;
    va_list args;
    int n = 1;

    argv[0] = (char*)arg;

    va_start(args, arg);

    for (;;) {
        if (n >= sizeof(argv) / sizeof(argv[0])) {
            errno = ENOMEM;
            return -1;
        }

        argn = va_arg(args, char*);
        argv[n++] = argn;
        if (!argv[n]) break;
    }

    va_end(args);

    return execve(path, argv, environ);
}

__endpoint_t get_endpoint()
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

int fork(void)
{
    MESSAGE msg;
    msg.type = FORK;
    msg.u.m_pm_clone.flags = 0;
    msg.u.m_pm_clone.stack = NULL;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.PID;
}

int vfork(void) { return fork(); }

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

void sigreturn(struct sigcontext* scp)
{
    MESSAGE msg;

    msg.type = PM_SIGRETURN;
    msg.MASK = scp->mask;
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
    msg.HOW = how;

    if (set)
        msg.MASK = *set;
    else
        msg.MASK = 0;

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

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
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

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int mknod(const char* pathname, mode_t mode, dev_t dev)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(MESSAGE));
    msg.type = MKNOD;
    msg.PATHNAME = (void*)pathname;
    msg.NAME_LEN = strlen(pathname);
    msg.MODE = mode;
    msg.FLAGS = dev;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int linkat(int fd1, const char* path1, int fd2, const char* path2, int flag)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = LINKAT;
    msg.u.m_vfs_linkat.fd1 = fd1;
    msg.u.m_vfs_linkat.path1 = (void*)path1;
    msg.u.m_vfs_linkat.path1_len = strlen(path1);
    msg.u.m_vfs_linkat.fd2 = fd2;
    msg.u.m_vfs_linkat.path2 = (void*)path2;
    msg.u.m_vfs_linkat.path2_len = strlen(path2);
    msg.u.m_vfs_linkat.flags = flag;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int link(const char* old, const char* new)
{
    return linkat(AT_FDCWD, old, AT_FDCWD, new, 0);
}

int unlinkat(int dirfd, const char* pathname, int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = UNLINKAT;
    msg.u.m_vfs_pathat.dirfd = dirfd;
    msg.u.m_vfs_pathat.pathname = (void*)pathname;
    msg.u.m_vfs_pathat.name_len = strlen(pathname);
    msg.u.m_vfs_pathat.flags = flags;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int unlink(const char* pathname) { return unlinkat(AT_FDCWD, pathname, 0); }

int rmdir(const char* pathname)
{
    return unlinkat(AT_FDCWD, pathname, AT_REMOVEDIR);
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
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETFL:
        msg.BUF_LEN = va_arg(argp, int);
        break;
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
        msg.BUF = va_arg(argp, void*);
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

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

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

int chroot(const char* path)
{
    printf("chroot: not implemented\n");
    errno = ENOSYS;
    return -1;
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

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int fchmod(int fd, mode_t mode)
{
    MESSAGE msg;
    msg.type = FCHMOD;

    msg.FD = fd;
    msg.MODE = mode;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = MOUNT;
    msg.u.m_vfs_mount.flags = mountflags;

    if (source == NULL)
        msg.u.m_vfs_mount.source_len = 0;
    else
        msg.u.m_vfs_mount.source_len = strlen(source);

    msg.u.m_vfs_mount.target_len = strlen(target);
    msg.u.m_vfs_mount.label_len = strlen(filesystemtype);
    msg.u.m_vfs_mount.source = (void*)source;
    msg.u.m_vfs_mount.target = (void*)target;
    msg.u.m_vfs_mount.label = (void*)filesystemtype;
    msg.u.m_vfs_mount.data = (void*)data;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
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

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int eaccess(const char* pathname, int mode) { return access(pathname, mode); }

off_t lseek(int fd, off_t offset, int whence)
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

    return (off_t)msg.OFFSET;
}

int openat(int fd, const char* pathname, int flags, ...)
{
    MESSAGE msg;
    va_list parg;

    memset(&msg, 0, sizeof(MESSAGE));
    va_start(parg, flags);

    msg.type = OPENAT;
    msg.u.m_vfs_pathat.dirfd = fd;
    msg.u.m_vfs_pathat.pathname = (void*)pathname;
    msg.u.m_vfs_pathat.name_len = strlen(pathname);
    msg.u.m_vfs_pathat.flags = flags;

    if (flags & O_CREAT) {
        msg.u.m_vfs_pathat.mode = va_arg(parg, mode_t);
    }

    va_end(parg);
    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}

int open(const char* pathname, int flags, ...)
{
    va_list parg;
    mode_t mode;

    if (flags & O_CREAT) {
        va_start(parg, flags);
        mode = va_arg(parg, mode_t);
        va_end(parg);

        return openat(AT_FDCWD, pathname, flags, mode);
    }

    return openat(AT_FDCWD, pathname, flags);
}

_READ_WRITE_RETURN_TYPE read(int fd, void* buf, size_t count)
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

ssize_t readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = READLINKAT;
    msg.u.m_vfs_pathat.dirfd = dirfd;
    msg.u.m_vfs_pathat.pathname = (void*)pathname;
    msg.u.m_vfs_pathat.name_len = strlen(pathname);
    msg.u.m_vfs_pathat.buf = buf;
    msg.u.m_vfs_pathat.buf_len = bufsiz;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

ssize_t readlink(const char* pathname, char* buf, size_t bufsiz)
{
    return readlinkat(AT_FDCWD, pathname, buf, bufsiz);
}

int ioctl(int fd, unsigned long request, ...)
{
    MESSAGE msg;
    va_list ap;
    void* data;

    va_start(ap, request);
    data = va_arg(ap, void*);
    va_end(ap);

    msg.type = IOCTL;
    msg.FD = fd;
    msg.u.m3.m3l1 = request;
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

int fstatat(int dirfd, const char* path, struct stat* buf, int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));

    msg.type = FSTATAT;
    msg.u.m_vfs_pathat.dirfd = dirfd;
    msg.u.m_vfs_pathat.pathname = (void*)path;
    msg.u.m_vfs_pathat.name_len = strlen(path);
    msg.u.m_vfs_pathat.buf = (void*)buf;
    msg.u.m_vfs_pathat.flags = flags;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

int _stat(const char* path, struct stat* buf)
{
    return fstatat(AT_FDCWD, path, buf, 0);
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

int _fstat(int fd, struct stat* buf)
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

int stat(const char* path, struct stat* buf) { return _stat(path, buf); }

int fstat(int fd, struct stat* buf) { return _fstat(fd, buf); }

int _stat64(const char* path, struct stat* buf) { return _stat(path, buf); }

int _fstat64(int fd, struct stat* buf) { return _fstat(fd, buf); }

int lstat(const char* path, struct stat* buf)
{
    return fstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

_READ_WRITE_RETURN_TYPE write(int fd, const void* buf, size_t count)
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

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

int symlink(const char* target, const char* linkpath)
{
    MESSAGE msg;
    msg.type = SYMLINK;
    msg.u.m_vfs_link.old_path = (char*)target;
    msg.u.m_vfs_link.new_path = (char*)linkpath;
    msg.u.m_vfs_link.old_path_len = strlen(target);
    msg.u.m_vfs_link.new_path_len = strlen(linkpath);

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}

int utimensat(int dirfd, const char* pathname, const struct timespec times[2],
              int flags)
{
    MESSAGE msg;
    static const struct timespec now[2] = {{0, UTIME_NOW}, {0, UTIME_NOW}};

    if (times == NULL) times = now;

    if (pathname == NULL) {
        if (dirfd == AT_FDCWD)
            errno = EFAULT;
        else
            errno = EINVAL;
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = UTIMENSAT;
    msg.u.m_vfs_utimensat.fd = dirfd;
    msg.u.m_vfs_utimensat.pathname = (void*)pathname;
    msg.u.m_vfs_utimensat.name_len = strlen(pathname);
    msg.u.m_vfs_utimensat.flags = flags;
    msg.u.m_vfs_utimensat.atime = times[0].tv_sec;
    msg.u.m_vfs_utimensat.mtime = times[1].tv_sec;
    msg.u.m_vfs_utimensat.ansec = times[0].tv_nsec;
    msg.u.m_vfs_utimensat.mnsec = times[1].tv_nsec;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int futimens(int fd, const struct timespec times[2])
{
    MESSAGE msg;
    static const struct timespec now[2] = {{0, UTIME_NOW}, {0, UTIME_NOW}};

    if (times == NULL) times = now;

    memset(&msg, 0, sizeof(msg));
    msg.type = UTIMENSAT;
    msg.u.m_vfs_utimensat.fd = fd;
    msg.u.m_vfs_utimensat.pathname = NULL;
    msg.u.m_vfs_utimensat.flags = 0;
    msg.u.m_vfs_utimensat.atime = times[0].tv_sec;
    msg.u.m_vfs_utimensat.mtime = times[1].tv_sec;
    msg.u.m_vfs_utimensat.ansec = times[0].tv_nsec;
    msg.u.m_vfs_utimensat.mnsec = times[1].tv_nsec;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int utime(const char* filename, const struct utimbuf* times)
{
    struct timespec ts[2];

    if (times == NULL) {
        ts[0].tv_sec = 0;
        ts[0].tv_nsec = UTIME_NOW;
        ts[1].tv_sec = 0;
        ts[1].tv_nsec = UTIME_NOW;
    } else {
        ts[0].tv_sec = times->actime;
        ts[0].tv_nsec = 0;
        ts[1].tv_sec = times->modtime;
        ts[1].tv_nsec = 0;
    }

    return utimensat(AT_FDCWD, filename, ts, 0);
}

int utimes(const char* filename, const struct timeval times[2])
{
    struct timespec ts[2];

    if (times == NULL) {
        ts[0].tv_sec = 0;
        ts[0].tv_nsec = UTIME_NOW;
        ts[1].tv_sec = 0;
        ts[1].tv_nsec = UTIME_NOW;
    } else {
        ts[0].tv_sec = times[0].tv_sec;
        ts[0].tv_nsec = times[0].tv_usec * 1000UL;
        ts[1].tv_sec = times[1].tv_sec;
        ts[1].tv_nsec = times[1].tv_usec * 1000UL;
    }

    return utimensat(AT_FDCWD, filename, ts, 0);
}

int fchownat(int dirfd, const char* pathname, uid_t owner, gid_t group,
             int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = FCHOWNAT;
    msg.u.m_vfs_fchownat.fd = dirfd;
    msg.u.m_vfs_fchownat.pathname = (void*)pathname;
    msg.u.m_vfs_fchownat.name_len = strlen(pathname);
    msg.u.m_vfs_fchownat.owner = owner;
    msg.u.m_vfs_fchownat.group = group;
    msg.u.m_vfs_fchownat.flags = flags;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int fchown(int fd, uid_t owner, gid_t group)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = FCHOWN;
    msg.u.m_vfs_fchownat.fd = fd;
    msg.u.m_vfs_fchownat.pathname = NULL;
    msg.u.m_vfs_fchownat.owner = owner;
    msg.u.m_vfs_fchownat.group = group;
    msg.u.m_vfs_fchownat.flags = 0;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL > 0) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

int chown(const char* path, uid_t owner, gid_t group)
{
    return fchownat(AT_FDCWD, path, owner, group, 0);
}

int lchown(const char* path, uid_t owner, gid_t group)
{
    return fchownat(AT_FDCWD, path, owner, group, AT_SYMLINK_NOFOLLOW);
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

void* sbrk(ptrdiff_t nbytes)
{
    char *oldsize = _brksize, *newsize = _brksize + nbytes;

    if ((nbytes < 0 && newsize > oldsize) || (nbytes > 0 && newsize < oldsize))
        return (void*)(-1);
    if (brk(newsize) == 0)
        return (void*)oldsize;
    else
        return (void*)(-1);
}

uid_t getuid(void)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETUID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return (uid_t)msg.RETVAL;
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

gid_t getgid(void)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETGID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return (gid_t)msg.RETVAL;
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

uid_t geteuid(void)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETEUID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return (uid_t)msg.RETVAL;
}

gid_t getegid(void)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_GETEGID;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return (gid_t)msg.RETVAL;
}

int seteuid(uid_t euid)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_SETEUID;
    msg.NEWID = euid;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    return msg.RETVAL;
}

int setegid(gid_t egid)
{
    MESSAGE msg;

    msg.type = GETSETID;
    msg.REQUEST = GS_SETEGID;
    msg.NEWID = egid;

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

int pipe2(int pipefd[2], int flags)
{
    MESSAGE msg;
    memset(&msg, 0, sizeof(MESSAGE));
    msg.type = PIPE2;
    msg.FLAGS = flags;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.u.m_vfs_fdpair.retval != 0) {
        errno = msg.u.m_vfs_fdpair.retval;
        return -1;
    }

    pipefd[0] = msg.u.m_vfs_fdpair.fd0;
    pipefd[1] = msg.u.m_vfs_fdpair.fd1;

    return 0;
}

int pipe(int pipefd[2]) { return pipe2(pipefd, 0); }

int dirfd(DIR* dirp) { return dirp->fd; }

void sync(void)
{
    MESSAGE msg;

    msg.type = SYNC;

    cmb();

    send_recv(BOTH, TASK_FS, &msg);
}

int fsync(int fd)
{
    puts("fsync: not implemented");
    return 0;
}

int fdatasync(int fd)
{
    puts("fdatasync: not implemented");
    return 0;
}

unsigned int alarm(unsigned int seconds)
{
    puts("alarm: not implemented");
    return 0;
}

clock_t times(struct tms* tp)
{
    struct rusage ru;
    struct timeval t;
    static clock_t clk_tck = 0;

    if (clk_tck == 0) {
        clk_tck = (clock_t)sysconf(_SC_CLK_TCK);
    }

#define CONVTCK(r) \
    (clock_t)(r.tv_sec * clk_tck + r.tv_usec / (1000000 / (uint)clk_tck))

    if (getrusage(RUSAGE_SELF, &ru) < 0) return (clock_t)-1;
    tp->tms_utime = CONVTCK(ru.ru_utime);
    tp->tms_stime = CONVTCK(ru.ru_stime);

    if (getrusage(RUSAGE_CHILDREN, &ru) < 0) return ((clock_t)-1);
    tp->tms_cutime = CONVTCK(ru.ru_utime);
    tp->tms_cstime = CONVTCK(ru.ru_stime);

    if (gettimeofday(&t, NULL)) return ((clock_t)-1);
    return ((clock_t)(CONVTCK(t)));
}

int getrusage(int who, struct rusage* usage)
{
    MESSAGE msg;

    msg.type = GETRUSAGE;
    msg.u.m3.m3i1 = who;
    msg.u.m3.m3p1 = usage;

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (msg.RETVAL) {
        errno = msg.RETVAL;
        return -1;
    }

    return 0;
}

struct group* getgrent(void)
{
    puts("getgrent: not implemented");
    return NULL;
}

void setgrent(void) { puts("setgrent: not implemented"); }

void endgrent(void) { puts("endgrent: not implemented"); }

long ptrace(int request, ...)
{
    MESSAGE msg;
    va_list argp;

    va_start(argp, request);

    memset(&msg, 0, sizeof(msg));
    msg.type = PTRACE;
    msg.PTRACE_REQ = request;
    msg.PTRACE_PID = va_arg(argp, pid_t);
    msg.PTRACE_ADDR = va_arg(argp, void*);

    if (request != PTRACE_CONT) msg.PTRACE_DATA = va_arg(argp, void*);

    va_end(argp);

    cmb();

    send_recv(BOTH, TASK_PM, &msg);

    if (msg.RETVAL != 0) {
        errno = msg.RETVAL;
        return -1;
    }

    errno = 0;
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

    if (m.u.m_mm_mmap_reply.retval) {
        errno = m.u.m_mm_mmap_reply.retval;
        return MAP_FAILED;
    }

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

void* mremap(void* old_addr, size_t old_size, size_t new_size, int flags, ...)
{
    MESSAGE m;
    va_list args;

    memset(&m, 0, sizeof(MESSAGE));

    m.type = MREMAP;
    m.u.m_mm_mremap.old_addr = old_addr;
    m.u.m_mm_mremap.old_size = old_size;
    m.u.m_mm_mremap.new_size = new_size;
    m.u.m_mm_mremap.flags = flags;
    m.u.m_mm_mremap.new_addr = NULL;

    if (flags & MREMAP_FIXED) {
        va_start(args, flags);
        m.u.m_mm_mremap.new_addr = va_arg(args, void*);
        va_end(args);
    }

    cmb();

    send_recv(BOTH, TASK_MM, &m);

    if (m.u.m_mm_mmap_reply.retval) {
        errno = m.u.m_mm_mmap_reply.retval;
        return MAP_FAILED;
    }

    return m.u.m_mm_mmap_reply.retaddr;
}

int ftruncate(int fd, off_t length)
{
    MESSAGE m;
    memset(&m, 0, sizeof(MESSAGE));

    m.type = FTRUNCATE;
    m.u.m_vfs_truncate.fd = fd;
    m.u.m_vfs_truncate.offset = length;

    cmb();
    send_recv(BOTH, TASK_FS, &m);

    if (m.RETVAL > 0) {
        errno = m.RETVAL;
        return -1;
    }

    return m.RETVAL;
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

int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
            const struct timespec* timeout, const sigset_t* sigmask)
{
    return select(nfds, readfds, writefds, exceptfds, NULL);
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
    m.FUTEX_VAL2 = (__u32)timeout;
    m.FUTEX_TIMEOUT = (__u32)timeout;
    m.FUTEX_UADDR2 = uaddr2;
    m.FUTEX_VAL3 = val3;

    cmb();

    send_recv(BOTH, TASK_PM, &m);

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

int shmctl(int shmid, int cmd, struct shmid_ds* buf)
{
    errno = ENOSYS;
    return -1;
}

int shmdt(const void* shmaddr)
{
    errno = ENOSYS;
    return -1;
}

int getentropy(void* buffer, size_t length)
{
    memset(buffer, 0, length);
    return 0;
}

extern void* memalign(size_t alignment, size_t size);

int posix_memalign(void** memptr, size_t alignment, size_t size)
{
    void* p = memalign(alignment, size);
    if (!p) return ENOMEM;

    *memptr = p;
    return 0;
}

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
