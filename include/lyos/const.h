/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _CONST_H_
#define _CONST_H_

/* return value */
#define OK 0

/* max() & min() */
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define roundup(x, align) \
    (((x) % align == 0) ? (x) : (((x) + align) - ((x) % align)))
#define rounddown(x, align) ((x) - ((x) % align))

/* GDT 和 IDT 中描述符的个数 */
#define IDT_SIZE 256

#define NR_LOCAL_SEGS 3
#define T             0
#define D             1
#define S             2

#define TRUE  1
#define FALSE 0

#define STR_DEFAULT_LEN 1024

#define NR_IOREQS 64

/* 8253/8254 PIT (Programmable Interval Timer) */
#define TIMER0     0x40 /* I/O port for timer channel 0 */
#define TIMER2     0x42 /* I/O port for timer channel 2 */
#define TIMER_MODE 0x43 /* I/O port for timer mode control */
#define RATE_GENERATOR                                                           \
    0x34                    /* 00-11-010-0 :                                     \
                             * Counter0 - LSB then MSB - rate generator - binary \
                             */
#define TIMER_FREQ 1193182L /* clock frequency for timer in PC and AT */
#define HZ         100      /* clock freq (software settable on IBM-PC) */
#define DEFAULT_HZ 100      /* clock freq (software settable on IBM-PC) */

/* VGA */
#define CRTC_ADDR_REG 0x3D4 /* CRT Controller Registers - Addr Register */
#define CRTC_DATA_REG 0x3D5 /* CRT Controller Registers - Data Register */
#define START_ADDR_H  0xC   /* reg index of video mem start addr (MSB) */
#define START_ADDR_L  0xD   /* reg index of video mem start addr (LSB) */
#define CURSOR_H      0xE   /* reg index of cursor position (MSB) */
#define CURSOR_L      0xF   /* reg index of cursor position (LSB) */

/* CMOS */
#define CLK_ELE                                         \
    0x70 /* CMOS RAM address register port (write only) \
          * Bit 7 = 1  NMI disable                      \
          *     0  NMI enable                           \
          * Bits 6-0 = RAM address                      \
          */

#define CLK_IO 0x71 /* CMOS RAM data register port (read/write) */

#define YEAR       9 /* Clock register addresses in CMOS RAM	*/
#define MONTH      8
#define DAY        7
#define HOUR       4
#define MINUTE     2
#define SECOND     0
#define CLK_STATUS 0x0B /* Status register B: RTC configuration	*/
#define CLK_HEALTH                                     \
    0x0E /* Diagnostic status: (should be set by Power \
          * On Self-Test [POST])                       \
          * Bit  7 = RTC lost power                    \
          *	6 = Checksum (for addr 0x10-0x2d) bad      \
          *	5 = Config. Info. bad at POST              \
          *	4 = Mem. size error at POST                \
          *	3 = I/O board failed initialization        \
          *	2 = CMOS time invalid                      \
          *    1-0 =    reserved                       \
          */

/* Hardware interrupts */
#define NR_IRQ         16 /* Number of IRQs */
#define CLOCK_IRQ      0
#define KEYBOARD_IRQ   1
#define CASCADE_IRQ    2 /* cascade enable for 2nd AT controller */
#define ETHER_IRQ      3 /* default ethernet interrupt vector */
#define SECONDARY_IRQ  3 /* RS232 interrupt vector for port 2 */
#define RS232_IRQ      4 /* RS232 interrupt vector for port 1 */
#define XT_WINI_IRQ    5 /* xt winchester */
#define FLOPPY_IRQ     6 /* floppy disk */
#define PRINTER_IRQ    7
#define CMOS_CLOCK_IRQ 8
#define PS_2_IRQ       12
#define AT_WINI_IRQ    14 /* at winchester */
#define AT_WINI_1_IRQ  15

/* tasks */
/* 注意 TASK_XXX 的定义要与 global.c 中对应 */
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
#ifdef __i386__
#define TASK_PCI 12
#define INIT     13
#else
#define INIT 12
#endif
#define ANY     (NR_PROCS + 10)
#define SELF    (-0x8ace)
#define NO_TASK (NR_PROCS + 20)

#define MAX_TICKS 0x7FFFABCD

/* system call */
#define NR_SYS_CALLS 23
#define NR_PRINTX    0
#define NR_SENDREC   1
#define NR_DATACOPY  2
#define NR_PRIVCTL   3
#define NR_GETINFO   4
#define NR_VMCTL     5
#define NR_UMAP      6
#define NR_PORTIO    7
#define NR_VPORTIO   8
#define NR_SPORTIO   9
#define NR_IRQCTL    10
#define NR_FORK      11
#define NR_CLEAR     12
#define NR_EXEC      13
#define NR_SIGSEND   14
#define NR_SIGRETURN 15
#define NR_KILL      16
#define NR_GETKSIG   17
#define NR_ENDKSIG   18
#define NR_TIMES     19
#define NR_TRACE     20
#define NR_ALARM     21
#define NR_KPROFILE  22

/* kernel signals */
#define SIGKSIG SIGUSR1
#define SIGKMEM SIGUSR2

/* ipc */
#define SEND          1
#define RECEIVE       2
#define BOTH          3 /* BOTH = (SEND | RECEIVE) */
#define SEND_NONBLOCK 4
#define SEND_ASYNC    5
#define RECEIVE_ASYNC 6

#define IPCF_FROMKERNEL 0x1
#define IPCF_NONBLOCK   0x2
#define IPCF_ASYNC      0x4

/* get/set id requests */
#define GS_GETUID  1
#define GS_SETUID  2
#define GS_GETGID  3
#define GS_SETGID  4
#define GS_GETEUID 5
#define GS_GETEGID 6
#define GS_GETEP   7
#define GS_GETPID  8
#define GS_GETPGRP 9
#define GS_SETSID  10
#define GS_GETPGID 11
#define GS_GETPPID 12
#define GS_GETTID  13

#define GS_GETHOSTNAME 1
#define GS_SETHOSTNAME 2

/* privilege control requests */
#define PRIVCTL_SET_PRIV 1
#define PRIVCTL_ALLOW    2

/* getinfo requests */
#define GETINFO_SYSINFO   1
#define GETINFO_KINFO     2
#define GETINFO_CMDLINE   3
#define GETINFO_BOOTPROCS 4
#define GETINFO_HZ        5
#define GETINFO_MACHINE   6
#define GETINFO_CPUINFO   7
#define GETINFO_PROCTAB   8
#define GETINFO_CPUTICKS  9

/* special message flags */
#define MSG_INTERRUPT 0x1 /* the process has an interrupt message */
#define MSG_KERNLOG   0x2 /* the process has a kernellog message */

#define VFS_REQ_BASE     1001
#define VFS_TXN_BASE     1101
#define IS_VFS_TXN_ID(t) ((t) >= VFS_TXN_BASE && (t) < MM_REQ_BASE)
#define MM_REQ_BASE      1201
#define PM_REQ_BASE      1501
#define DRV_REQ_BASE     2001
#define SYSFS_REQ_BASE   2100
#define SERVMAN_REQ_BASE 2201
#define PCI_REQ_BASE     2301
#define INPUT_REQ_BASE   2401
#define IPC_REQ_BASE     2501

#define IS_BDEV_REQ(x) ((x) >= BDEV_OPEN && (x) <= BDEV_IOCTL)
#define IS_CDEV_REQ(x) ((x) >= CDEV_OPEN && (x) <= CDEV_IOCTL)

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
    STTY,
    UNAME, /* 11 */
    PROF,
    PHYS,
    LOCK,
    MPX,
    GET_TIME_OF_DAY,
    GETSETHOSTNAME, /* 17 */
    OPEN,
    CLOSE,
    READ,
    WRITE,
    LSEEK,
    STAT,
    FSTAT,
    UNLINK, /* 25 */
    MOUNT,
    UMOUNT,
    MKDIR,
    CHROOT,
    CHDIR,
    FCHDIR,
    ACCESS, /* 32 */
    UMASK,
    DUP,
    IOCTL,
    FCNTL,
    CHMOD,
    FCHMOD,
    GETDENTS, /* 39 */
    _SUSPEND_PROC,
    _RESUME_PROC,
    EXEC,
    WAIT,
    KILL,
    ACCT, /* 45 */
    BRK,
    GETSETID,
    ALARM,
    SIGACTION,
    SIGPROCMASK,
    SIGSUSPEND, /* 51 */
    PROCCTL,
    MMAP,
    FORK,
    EXIT,
    MUNMAP,
    FUTEX,
    SELECT, /* 58 */
    SYNC,
    READLINK,
    LSTAT, /* 61 */
    SYMLINK,
    SIGNALFD,

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

    /* message for sysfs */
    SYSFS_PUBLISH = SYSFS_REQ_BASE, /* 2100 */
    SYSFS_RETRIEVE,
    SYSFS_DYN_SHOW,
    SYSFS_DYN_STORE,
    SYSFS_PUBLISH_LINK,

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
    PCI_ATTR_W16,
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
};

/* macros for messages */
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

/* macros for sendrec */
#define SR_FUNCTION u.m3.m3i1
#define SR_SRCDEST  u.m3.m3i2
#define SR_MSG      u.m3.m3p1
#define SR_TABLE    u.m3.m3p1
#define SR_LEN      u.m3.m3i2

/* macros for data_copy */
#define SRC_EP    u.m3.m3i1
#define SRC_ADDR  u.m3.m3p1
#define SRC_SEG   u.m3.m3l1
#define DEST_EP   u.m3.m3i2
#define DEST_ADDR u.m3.m3p2
#define DEST_SEG  u.m3.m3l2

/* macros for mount */
#define MSOURCE   u.m4.m4p1
#define MTARGET   u.m4.m4p2
#define MLABEL    u.m4.m4p3
#define MDATA     u.m4.m4p4
#define MFLAGS    u.m4.m4l1
#define MNAMELEN1 u.m4.m4i1
#define MNAMELEN2 u.m4.m4i2
#define MNAMELEN3 u.m4.m4i3

/* macros for fault */
#define FAULT_NR      u.m3.m3i1
#define FAULT_ADDR    u.m3.m3p1
#define FAULT_PROC    u.m3.m3i3
#define FAULT_ERRCODE u.m3.m3i4

/* macros for futex */
#define FUTEX_OP      u.m3.m3i1
#define FUTEX_VAL     u.m3.m3i2
#define FUTEX_VAL2    u.m3.m3i3
#define FUTEX_VAL3    u.m3.m3i4
#define FUTEX_UADDR   u.m3.m3p1
#define FUTEX_UADDR2  u.m3.m3p2
#define FUTEX_TIMEOUT u.m3.m3l1

/* macros for kprofile */
#define KP_ACTION u.m3.m3i1
#define KP_SIZE   u.m3.m3i2
#define KP_FREQ   u.m3.m3i3
#define KP_ENDPT  u.m3.m3i4
#define KP_CTL    u.m3.m3p1
#define KP_BUF    u.m3.m3p2

/* macros for ipc calls */
#define IPC_KEY     u.m3.m3i1
#define IPC_ID      u.m3.m3i1
#define IPC_SIZE    u.m3.m3i2
#define IPC_FLAGS   u.m3.m3i3
#define IPC_ADDR    u.m3.m3p1
#define IPC_RETID   u.m3.m3i2
#define IPC_RETADDR u.m3.m3p2

/* macros for getepinfo */
#define EP_EFFUID u.m3.m3i2
#define EP_EFFGID u.m3.m3i3

/* ioctl requests */
#define DIOCTL_GET_GEO 1

/* Hard Drive */
#define SECTOR_SIZE       512
#define SECTOR_BITS       (SECTOR_SIZE * 8)
#define SECTOR_SIZE_SHIFT 9

/* TODO: device major dynamic allocation */
#define NO_DEV       0
#define DEV_RD       1
#define DEV_FLOPPY   2
#define DEV_HD       3
#define DEV_CHAR_TTY 4
#define DEV_INPUT    13
#define DEV_CHAR_FB  29
#define DEV_DRM      34
#define DEV_VD       112

/* make device number from major and minor numbers */
#define MAJOR_SHIFT    8
#define MINOR_MASK     ((1U << MAJOR_SHIFT) - 1)
#define MAKE_DEV(a, b) (((a) << MAJOR_SHIFT) | (b))
/* separate major and minor numbers from device number */
#define MAJOR(x) ((dev_t)(x >> MAJOR_SHIFT))
#define MINOR(x) ((dev_t)(x & MINOR_MASK))

#define INVALID_INODE 0
#define ROOT_INODE    1

#define MINOR_INITRD 120

#define NR_FILES     64
#define NR_FILE_DESC 64 /* FIXME */
#define NR_INODE     64 /* FIXME */
#define NR_VFS_MOUNT 16

#define NO_BLOCK ((block_t)0)

/* INODE::i_mode (octal, lower 12 bits reserved) */
/* Flag bits for i_mode in the inode. */
#define I_TYPE_MASK     I_TYPE
#define I_TYPE          0170000 /* this field gives inode type */
#define I_UNIX_SOCKET   0140000 /* unix domain socket */
#define I_SYMBOLIC_LINK 0120000 /* file is a symbolic link */
#define I_REGULAR       0100000 /* regular file, not dir or special */
#define I_BLOCK_SPECIAL 0060000 /* block special file */
#define I_DIRECTORY     0040000 /* file is a directory */
#define I_CHAR_SPECIAL  0020000 /* character special file */
#define I_NAMED_PIPE    0010000 /* named pipe (FIFO) */
#define I_SET_UID_BIT   0004000 /* set effective uid_t on exec */
#define I_SET_GID_BIT   0002000 /* set effective gid_t on exec */
#define I_SET_STCKY_BIT 0001000 /* sticky bit */
#define ALL_MODES       0007777 /* all bits for user, group and others */
#define RWX_MODES       0000777 /* mode bits for RWX only */
#define R_BIT           0000004 /* Rwx protection bit */
#define W_BIT           0000002 /* rWx protection bit */
#define X_BIT           0000001 /* rwX protection bit */
#define I_NOT_ALLOC     0000000 /* this inode is free */

#define is_special(m)                          \
    ((((m)&I_TYPE_MASK) == I_BLOCK_SPECIAL) || \
     (((m)&I_TYPE_MASK) == I_CHAR_SPECIAL))

#define NR_DEFAULT_FILE_SECTS 2048 /* 2048 * 512 = 1MB */

#define suser() (current->euid == 0)

/* super user uid */
#define SU_UID ((uid_t)0)

#endif /* _CONST_H_ */
