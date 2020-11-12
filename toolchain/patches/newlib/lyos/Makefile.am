AUTOMAKE_OPTIONS = cygnus
INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)
AM_CCASFLAGS = $(INCLUDES)

SUBDIRS = machine \
	$(EXTRA_SUBDIRS)
	.

noinst_LIBRARIES = lib.a

SUBLIBS = $(LYOS_MACHINE_LIB) \
	$(EXTRA_SUBLIBS)

GENERAL_SOURCES = syscalls.c init.c getpass.c getpwent.c getcwd.c ttyname_r.c ttyname.c \
				mntent.c err.c progname.c crypt3.c sysconf.c regex.c ucontext.c poll.c \
				clock_gettime.c clock_settime.c clock_getres.c sched.c usleep.c \
				nanosleep.c signalfd.c flockfile.c popen.c eventfd.c timerfd.c epoll.c \
				socket.c dl.c iconv.c fnmatch.c scandir.c ftw.c prctl.c wait.c basename.c \
				inet.c netdb.c dirent.c uio.c inotify.c realpath.c dirname.c random.c \
				fcntl.c grp.c tls.c pty.c pathconf.c

ADD_OBJS = $(LYOS_MACHINE_LIB)

# liblyos_la_LDFLAGS = -Xcompiler -nostdlib

if USE_LIBTOOL
noinst_LTLIBRARIES = liblyos.la
liblyos_la_SOURCES = $(GENERAL_SOURCES)
liblyos_la_LIBADD = $(ADD_OBJS)
liblyos_la_DEPENDENCIES = $(ADD_OBJS)
LIBC_COMPILE = $(LTCOMPILE)
noinst_DATA = objectlist.awk.in
else
lib_a_SOURCES = $(GENERAL_SOURCES)
lib_a_LIBADD = $(ADD_OBJS)
lib_a_DEPENDENCIES = $(ADD_OBJS)
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)
LIBC_COMPILE = $(COMPILE)
noinst_DATA =

lib.a: $(lib_a_OBJECTS)
	rm -f $@
	rm -rf tmp
	mkdir tmp
	cd tmp; \
	 for i in $(SUBLIBS); do \
	   $(AR) x ../$$i; \
	 done; \
	$(AR) $(AR_FLAGS) ../$@ *.o ../lib_a-*.o
	$(RANLIB) $@
	rm -rf tmp

endif # USE_LIBTOOL

objectlist.awk.in: $(noinst_LTLIBRARIES) $(SUBLIBS)
	-rm -f objectlist.awk.in
	for i in `ls *.lo` ; \
	  do \
		echo $$i `pwd`/$$i >> objectlist.awk.in ; \
	done
	for j in $(SUBLIBS) ; \
	  do \
		export i=`echo $$j | sed -e 's,\(.*\)\/[^\/]*$$,\1,'`; \
		if test $$i != "." && test -f $$i/objectlist.awk.in; then \
			cat $$i/objectlist.awk.in >> objectlist.awk.in ; \
		fi; \
	done

all-local: crt0.o crti.o crtn.o

crt0.o: machine/crt0.o
	rm -f $@
	ln machine/crt0.o $@ >/dev/null 2>/dev/null || cp machine/crt0.o $@
crti.o: machine/crti.o
	rm -f $@
	ln machine/crti.o $@ >/dev/null 2>/dev/null || cp machine/crti.o $@
crtn.o: machine/crtn.o
	rm -f $@
	ln machine/crtn.o $@ >/dev/null 2>/dev/null || cp machine/crtn.o $@

machine/crt0.o:; @true
machine/crti.o:; @true
machine/crtn.o:; @true
machine/lib.a:; @true

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(tooldir)/include/arpa; \
	for i in $(srcdir)/include/arpa/*.h; do \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(tooldir)/include/arpa/`basename $$i`; \
	done; \
	$(mkinstalldirs) $(DESTDIR)$(tooldir)/include/netinet; \
	for i in $(srcdir)/include/netinet/*.h; do \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(tooldir)/include/netinet/`basename $$i`; \
	done;
	$(mkinstalldirs) $(DESTDIR)$(tooldir)/include/net; \
	for i in $(srcdir)/include/net/*.h; do \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(tooldir)/include/net/`basename $$i`; \
	done;

AM_CFLAGS = -I $(srcdir)/../../stdio
ACLOCAL_AMFLAGS = -I ../../.. -I ../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
