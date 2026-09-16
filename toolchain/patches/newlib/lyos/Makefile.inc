libc_a_CPPFLAGS += \
	-I$(srcdir)/libc/sys/lyos/include \
	-I$(srcdir)/libc/sys/lyos \
	-I$(srcdir)/libc/sys/lyos/posix \
	-I$(srcdir)/libc/sys/lyos/pthread

libc_a_SOURCES += \
	%D%/basename.c %D%/clock_getres.c %D%/clock_gettime.c \
	%D%/clock_settime.c %D%/crypt3.c %D%/dirent.c %D%/dirname.c \
	%D%/dl.c %D%/epoll.c %D%/err.c %D%/eventfd.c %D%/fcntl.c \
	%D%/flockfile.c %D%/fnmatch.c %D%/ftw.c %D%/getcwd.c \
	%D%/getpass.c %D%/getpwent.c %D%/gettimeofday.c %D%/grp.c \
	%D%/iconv.c %D%/inet.c %D%/inet_addr.c %D%/inet_ntop.c \
	%D%/inet_pton.c \
	%D%/init.c %D%/inotify.c %D%/mntent.c %D%/nanosleep.c \
	%D%/netdb.c %D%/pathconf.c %D%/poll.c %D%/popen.c %D%/prctl.c \
	%D%/progname.c %D%/pty.c %D%/random.c %D%/realpath.c \
	%D%/scandir.c %D%/sched.c %D%/signal.c %D%/signalfd.c \
	%D%/socket.c %D%/syscalls.c %D%/sysconf.c %D%/syslog.c \
	%D%/timerfd.c %D%/tls_get_addr.c %D%/ttyname.c %D%/ttyname_r.c \
	%D%/ucontext.c %D%/uio.c %D%/usleep.c %D%/wait.c \
	%D%/posix/collate.c %D%/posix/collcmp.c %D%/posix/regcomp.c \
	%D%/posix/regerror.c %D%/posix/regexec.c %D%/posix/regfree.c \
	%D%/posix/posix_spawn.c \
	%D%/pthread/pthread.c %D%/pthread/pthread_attr.c \
	%D%/pthread/pthread_barrier.c %D%/pthread/pthread_cond.c \
	%D%/pthread/pthread_create.c %D%/pthread/pthread_join.c \
	%D%/pthread/pthread_mutex.c %D%/pthread/pthread_once.c \
	%D%/pthread/pthread_rwlock.c %D%/pthread/pthread_specific.c \
	%D%/pthread/pthread_cleanup.c

if HAVE_LIBC_MACHINE_AARCH64
libc_a_SOURCES += \
	%D%/machine/aarch64/gate_intr.S \
	%D%/machine/aarch64/getpagesize.c \
	%D%/machine/aarch64/__sigreturn.S \
	%D%/machine/aarch64/__ucontext.S \
	%D%/machine/aarch64/makecontext.c \
	%D%/machine/aarch64/clone.c \
	%D%/machine/aarch64/tls.c
endif

if HAVE_LIBC_MACHINE_ARM
libc_a_SOURCES += \
	%D%/machine/arm/gate_intr.S %D%/machine/arm/__sigreturn.S \
	%D%/machine/arm/__udivsi3.c %D%/machine/arm/__umodsi3.c \
	%D%/machine/arm/__clzsi2.c %D%/machine/arm/adddf3.c \
	%D%/machine/arm/eqdf2.c %D%/machine/arm/ledf2.c \
	%D%/machine/arm/gedf2.c %D%/machine/arm/muldf3.c \
	%D%/machine/arm/divdf3.c %D%/machine/arm/subdf3.c \
	%D%/machine/arm/floatsidf.c %D%/machine/arm/floatunsidf.c \
	%D%/machine/arm/fixdfsi.c %D%/machine/arm/__divsi3.c \
	%D%/machine/arm/__modsi3.c %D%/machine/arm/clone.c
endif

if HAVE_LIBC_MACHINE_I386
libc_a_SOURCES += \
	%D%/machine/i386/__sigreturn.S %D%/machine/i386/gate_intr.S \
	%D%/machine/i386/clone.c %D%/machine/i386/__clone.S \
	%D%/machine/i386/getpagesize.c %D%/machine/i386/__ucontext.S \
	%D%/machine/i386/makecontext.c \
	%D%/machine/i386/set_thread_area.c %D%/machine/i386/tls.c
endif

if HAVE_LIBC_MACHINE_RISCV
libc_a_SOURCES += \
	%D%/machine/riscv/_init.S %D%/machine/riscv/gate_intr.S \
	%D%/machine/riscv/__sigreturn.S %D%/machine/riscv/clone.c \
	%D%/machine/riscv/getpagesize.c %D%/machine/riscv/__ucontext.S \
	%D%/machine/riscv/makecontext.c %D%/machine/riscv/tls.c
endif

if HAVE_LIBC_MACHINE_X86_64
libc_a_SOURCES += \
	%D%/machine/x86_64/__sigreturn.S \
	%D%/machine/x86_64/gate_intr.S \
	%D%/machine/x86_64/clone.c %D%/machine/x86_64/__clone.S \
	%D%/machine/x86_64/getpagesize.c \
	%D%/machine/x86_64/__ucontext.S \
	%D%/machine/x86_64/makecontext.c \
	%D%/machine/x86_64/set_thread_area.c \
	%D%/machine/x86_64/tls.c %D%/machine/x86_64/arch_prctl.c
endif
