#ifndef _UAPI_LYOS_CONST_H_
#define _UAPI_LYOS_CONST_H_

/* Task numbers, see kernel/global.c. */
#define INVALID_DRIVER -20
#define IDLE           -19
#define CLOCK          -4
#define SYSTEM         -3
#define KERNEL         -2
#define INTERRUPT      -1
#define TASK_MM        0
#define TASK_PM        1
#define TASK_SERVMAN   2
#define TASK_DEVMAN    3
#define TASK_SCHED     4
#define TASK_FS        5
#define TASK_SYS       6
#define TASK_TTY       7
#define TASK_RD        8
#define TASK_INITFS    9
#define TASK_SYSFS     10
#define TASK_IPC       11
#define TASK_NETLINK   12
#define INIT           13
#define SELF           (-0x8ace)

/* System call numbers. */
#define NR_SYS_CALLS       29
#define NR_PRINTX          0
#define NR_SENDREC         1
#define NR_DATACOPY        2
#define NR_PRIVCTL         3
#define NR_GETINFO         4
#define NR_VMCTL           5
#define NR_UMAP            6
#define NR_PORTIO          7
#define NR_VPORTIO         8
#define NR_SPORTIO         9
#define NR_IRQCTL          10
#define NR_FORK            11
#define NR_CLEAR           12
#define NR_EXEC            13
#define NR_SIGSEND         14
#define NR_SIGRETURN       15
#define NR_KILL            16
#define NR_GETKSIG         17
#define NR_ENDKSIG         18
#define NR_TIMES           19
#define NR_TRACE           20
#define NR_ALARM           21
#define NR_KPROFILE        22
#define NR_SETGRANT        23
#define NR_SAFECOPYFROM    24
#define NR_SAFECOPYTO      25
#define NR_SET_THREAD_AREA 26
#define NR_STIME           27
#define NR_ARCH_PRCTL      28

/* For send_recv(). */
#define SEND          1
#define RECEIVE       2
#define BOTH          3 /* BOTH = (SEND | RECEIVE) */
#define SEND_NONBLOCK 4
#define SEND_ASYNC    5
#define RECEIVE_ASYNC 6
#define NOTIFY        7

#define IPCF_FROMKERNEL 0x1
#define IPCF_NONBLOCK   0x2
#define IPCF_ASYNC      0x4

/* Get/set ID requests. */
#define GS_GETUID    1
#define GS_SETUID    2
#define GS_GETGID    3
#define GS_SETGID    4
#define GS_GETEUID   5
#define GS_GETEGID   6
#define GS_GETEP     7
#define GS_GETPID    8
#define GS_GETPGRP   9
#define GS_SETSID    10
#define GS_GETPGID   11
#define GS_GETPPID   12
#define GS_GETTID    13
#define GS_GETGROUPS 14
#define GS_SETGROUPS 15
#define GS_SETEUID   16
#define GS_SETEGID   17

#define GS_GETHOSTNAME 1
#define GS_SETHOSTNAME 2

/* Privilege control requests. */
#define PRIVCTL_SET_PRIV 1
#define PRIVCTL_ALLOW    2

/* getinfo requests. */
#define GETINFO_SYSINFO   1
#define GETINFO_KINFO     2
#define GETINFO_CMDLINE   3
#define GETINFO_BOOTPROCS 4
#define GETINFO_HZ        5
#define GETINFO_MACHINE   6
#define GETINFO_CPUINFO   7
#define GETINFO_PROCTAB   8
#define GETINFO_CPUTICKS  9

#define VFS_REQ_BASE     1001
#define VFS_TXN_BASE     1101
#define MM_REQ_BASE      1201
#define PM_REQ_BASE      1501
#define DRV_REQ_BASE     2001
#define SYSFS_REQ_BASE   2100
#define SERVMAN_REQ_BASE 2201
#define PCI_REQ_BASE     2301
#define INPUT_REQ_BASE   2401
#define IPC_REQ_BASE     2501
#define NETLINK_REQ_BASE 2601
#define DEVPTS_BASE      2701

/**
 * @enum msgtype
 * @brief MESSAGE types
 */
enum msgtype {
    /*
     * when hard interrupt occurs, a msg (with type==HARD_INT) will
     * be sent to some tasks
     */
    NOTIFY_MSG = 1, /* 1 */
    /*
     * when exception occurs, a msg (with type==FAULT) will
     * be sent to some tasks
     */
    FAULT, /* 2 */

    SYSCALL_RET, /* 3 */

    /* message type for syscalls */
    GET_TICKS,
    GET_RTC_TIME,
    FTIME,
    BREAK,
    PTRACE,
    GTTY,
    STTY, /* 10 */
    UNAME,
    PROF,
    PHYS,
    LOCK,
    MPX,
    GET_TIME_OF_DAY,
    GETSETHOSTNAME,
    OPEN,
    CLOSE,
    READ, /* 20 */
    WRITE,
    LSEEK,
    STAT,
    FSTAT,
    UNLINK,
    MOUNT,
    UMOUNT,
    MKDIR,
    CHROOT,
    CHDIR, /* 30 */
    FCHDIR,
    ACCESS,
    UMASK,
    DUP,
    IOCTL,
    FCNTL,
    CHMOD,
    FCHMOD,
    GETDENTS,
    _SUSPEND_PROC, /* 40 */
    _RESUME_PROC,
    EXEC,
    WAIT,
    KILL,
    ACCT,
    BRK,
    GETSETID,
    ALARM,
    SIGACTION,
    SIGPROCMASK, /* 50 */
    SIGSUSPEND,
    PROCCTL,
    MMAP,
    FORK,
    EXIT,
    MUNMAP,
    FUTEX,
    SELECT,
    SYNC,
    READLINK, /* 60 */
    LSTAT,
    SYMLINK,
    SIGNALFD,
    PIPE2,
    GETRUSAGE,
    POLL,
    EVENTFD,
    TIMERFD_CREATE,
    TIMERFD_SETTIME,
    TIMERFD_GETTIME, /* 70 */
    EPOLL_CREATE1,
    EPOLL_CTL,
    EPOLL_WAIT,
    SOCKET,
    RMDIR,
    BIND,
    CONNECT,
    LISTEN,
    ACCEPT4,
    SENDTO, /* 80 */
    RECVFROM,
    SENDMSG,
    RECVMSG,
    GETSOCKOPT,
    SOCKETPAIR,
    SETSOCKOPT,
    MKNOD,
    OPENAT,
    FSTATAT,
    UNLINKAT, /* 90 */
    READLINKAT,
    UTIMENSAT,
    INOTIFY_INIT1,
    INOTIFY_ADD_WATCH,
    INOTIFY_RM_WATCH,
    FCHOWN,
    FCHOWNAT,
    GETSOCKNAME,
    LINKAT,
    FTRUNCATE, /* 100 */
    MREMAP,
    CLOCK_SETTIME,

    /* DEVMAN */
    DM_DEVICE_ADD = 500,
    DM_GET_DRIVER, /* 54 ~ 55 */
    DM_BUS_REGISTER,
    DM_CLASS_REGISTER,
    DM_DEVICE_REGISTER,
    DM_BUS_ATTR_ADD,
    DM_BUS_ATTR_SHOW,
    DM_BUS_ATTR_STORE,
    DM_DEVICE_ATTR_ADD,
    DM_DEVICE_ATTR_SHOW,
    DM_DEVICE_ATTR_STORE,

    /* message type for fs request */
    FSREQ_RET = VFS_REQ_BASE,
    FS_REGISTER,
    FS_PUTINODE, /* 1001 ~ 1011 */
    FS_LOOKUP,
    FS_MOUNTPOINT,
    FS_READSUPER,
    FS_STAT,
    FS_RDWT,
    FS_CREATE,
    FS_FTRUNC,
    FS_SYNC,
    FS_MMAP,
    FS_CHMOD,
    FS_GETDENTS,
    FS_THREAD_WAKEUP,
    FS_MKDIR,
    FS_RDLINK,
    FS_SYMLINK,
    FS_UNLINK,
    FS_RMDIR,
    FS_MKNOD,
    FS_UTIME,
    FS_CHOWN,
    FS_LINK,

    VFS_MAPDRIVER,
    VFS_SOCKETPATH,
    VFS_COPYFD,

    FS_TXN_ID = VFS_TXN_BASE,

    /* message type for mm calls */
    MM_MAP_PHYS = MM_REQ_BASE,
    MM_VFS_REQUEST,
    MM_VFS_REPLY,
    MM_GETINFO,
    MM_REMAP,

    /* message type for pm calls */
    PM_VFS_INIT = PM_REQ_BASE, /* 1501 */
    PM_MM_FORK,
    PM_VFS_FORK,
    PM_SIGRETURN,
    PM_VFS_GETSETID,
    PM_GETPROCEP,
    PM_VFS_GETSETID_REPLY,
    PM_VFS_EXEC,
    PM_GETINFO,
    PM_KPROFILE,
    PM_GETEPINFO,
    PM_SIGNALFD_REPLY,
    PM_SIGNALFD_DEQUEUE,
    PM_SIGNALFD_GETNEXT,
    PM_SIGNALFD_POLL_NOTIFY,

    /* message type for drivers */ /* 2001 ~ 2006 */
    BDEV_OPEN = DRV_REQ_BASE,
    BDEV_CLOSE,
    BDEV_READ,
    BDEV_WRITE,
    BDEV_READV,
    BDEV_WRITEV,
    BDEV_IOCTL,
    BDEV_REPLY,

    CDEV_OPEN, /* 2007 ~ 2011 */
    CDEV_CLOSE,
    CDEV_READ,
    CDEV_WRITE,
    CDEV_IOCTL,
    CDEV_MMAP,
    CDEV_SELECT,
    CDEV_REPLY,
    CDEV_POLL_NOTIFY,

    SDEV_SOCKET,
    SDEV_SOCKETPAIR,
    SDEV_BIND,
    SDEV_CONNECT,
    SDEV_LISTEN,
    SDEV_ACCEPT,
    SDEV_SEND,
    SDEV_RECV,
    SDEV_VSEND,
    SDEV_VRECV,
    SDEV_SELECT,
    SDEV_GETSOCKOPT,
    SDEV_SETSOCKOPT,
    SDEV_GETSOCKNAME,
    SDEV_CLOSE,
    SDEV_REPLY,
    SDEV_SOCKET_REPLY,
    SDEV_ACCEPT_REPLY,
    SDEV_RECV_REPLY,
    SDEV_POLL_NOTIFY,

    /* message for sysfs */
    SYSFS_PUBLISH = SYSFS_REQ_BASE, /* 2100 */
    SYSFS_RETRIEVE,
    SYSFS_DYN_SHOW,
    SYSFS_DYN_STORE,
    SYSFS_PUBLISH_LINK,
    SYSFS_SUBSCRIBE,
    SYSFS_GET_EVENT,

    /* SERVMAN */
    SERVICE_UP = SERVMAN_REQ_BASE,
    SERVICE_DOWN,
    SERVICE_INIT,
    SERVICE_INIT_REPLY,

    PCI_SET_ACL = PCI_REQ_BASE,
    PCI_FIRST_DEV,
    PCI_NEXT_DEV,
    PCI_ATTR_R8,
    PCI_ATTR_R16,
    PCI_ATTR_R32,
    PCI_ATTR_W8,
    PCI_ATTR_W16,
    PCI_ATTR_W32,
    PCI_GET_BAR,
    PCI_FIND_CAPABILITY,
    PCI_FIND_NEXT_CAPABILITY,

    /* INPUT */
    INPUT_SEND_EVENT = INPUT_REQ_BASE,
    INPUT_TTY_UP,
    INPUT_TTY_EVENT,
    INPUT_SETLEDS,
    INPUT_REGISTER_DEVICE,
    INPUT_CONF,

    /* IPC */
    IPC_SHMGET = IPC_REQ_BASE,
    IPC_SHMAT,

    DEVPTS_CLEAR = DEVPTS_BASE,
    DEVPTS_SET,
};

/* Macros for messages. */
#define FD         u.m3.m3i1
#define NEWFD      u.m3.m3i2
#define PATHNAME   u.m3.m3p1
#define FLAGS      u.m3.m3i1
#define NAME_LEN   u.m3.m3i2
#define BUF_LEN    u.m3.m3i3
#define MODE       u.m3.m3i4
#define CNT        u.m3.m3i2
#define NEWID      u.m3.m3i1
#define UID        u.m3.m3i1
#define EUID       u.m3.m3i2
#define GID        u.m3.m3i1
#define EGID       u.m3.m3i2
#define NEWUID     u.m3.m3i1
#define NEWGID     u.m3.m3i2
#define REQUEST    u.m3.m3i2
#define PROC_NR    u.m3.m3i3
#define PENDPOINT  u.m3.m3i3
#define ENDPOINT   u.m3.m3i4
#define DEVICE     u.m3.m3i4
#define POSITION   u.m3.m3l1
#define ADDR       u.m3.m3p2
#define BUF        u.m3.m3p2
#define OFFSET     u.m3.m3i2
#define WHENCE     u.m3.m3i3
#define SECONDS    u.m3.m3i1
#define SIGNR      u.m3.m3i1
#define SIGSET     u.m3.m3i1
#define MASK       u.m3.m3i1
#define HOW        u.m3.m3i2
#define OPTIONS    u.m3.m3i1
#define NEWSA      u.m3.m3p1
#define OLDSA      u.m3.m3p2
#define SIGRET     u.m3.m3l1
#define TARGET     u.m3.m3i4
#define INTERRUPTS u.m3.m3l1
#define PID        u.m3.m3i2
#define RETVAL     u.m3.m3i1
#define STATUS     u.m3.m3i1
#define TIMESTAMP  u.m3.m3l1
#define ATTRID     u.m3.m3i3

/* Macros for sendrec(). */
#define SR_FUNCTION u.m3.m3i1
#define SR_SRCDEST  u.m3.m3i2
#define SR_MSG      u.m3.m3p1
#define SR_TABLE    u.m3.m3p1
#define SR_LEN      u.m3.m3i2

/* Macros for futex. */
#define FUTEX_OP      u.m3.m3i1
#define FUTEX_VAL     u.m3.m3i2
#define FUTEX_VAL2    u.m3.m3i3
#define FUTEX_VAL3    u.m3.m3i4
#define FUTEX_UADDR   u.m3.m3p1
#define FUTEX_UADDR2  u.m3.m3p2
#define FUTEX_TIMEOUT u.m3.m3l1

/* Macros for kprofile. */
#define KP_ACTION u.m3.m3i1
#define KP_SIZE   u.m3.m3i2
#define KP_FREQ   u.m3.m3i3
#define KP_ENDPT  u.m3.m3i4
#define KP_CTL    u.m3.m3p1
#define KP_BUF    u.m3.m3p2

/* Macros for IPC calls. */
#define IPC_KEY     u.m3.m3i1
#define IPC_ID      u.m3.m3i1
#define IPC_SIZE    u.m3.m3i2
#define IPC_FLAGS   u.m3.m3i3
#define IPC_ADDR    u.m3.m3p1
#define IPC_RETID   u.m3.m3i2
#define IPC_RETADDR u.m3.m3p2

/* Macros for getepinfo(). */
#define EP_EFFUID u.m3.m3i2
#define EP_EFFGID u.m3.m3i3

/* ioctl requests */
#define DIOCTL_GET_GEO 1

#endif
