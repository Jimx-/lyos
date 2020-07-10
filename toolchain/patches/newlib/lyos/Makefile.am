AUTOMAKE_OPTIONS = cygnus
INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)
AM_CCASFLAGS = $(INCLUDES)

SUBDIRS = machine .

noinst_LIBRARIES = lib.a

if MAY_SUPPLY_SYSCALLS
extra_objs = $(lpfx)syscalls.o $(lpfx)init.o
else
extra_objs =
endif

SUBLIBS = ${LYOS_MACHINE_LIB}

lib_a_SOURCES = syscalls.c init.c getpass.c getpwent.c getcwd.c ttyname_r.c ttyname.c \
				mntent.c err.c progname.c crypt3.c sysconf.c regex.c ucontext.c poll.c
lib_a_LIBADD = $(extra_objs) machine/lib.a
lib_a_DEPENDENCIES = $(extra_objs) machine/lib.a
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)

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

ACLOCAL_AMFLAGS = -I ../../.. -I ../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
