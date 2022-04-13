AUTOMAKE_OPTIONS = cygnus

INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)
AM_CCASFLAGS = $(INCLUDES)

LIB_SOURCES = gate_intr.S getpagesize.c __sigreturn.S __ucontext.S makecontext.c \
				clone.c

# liblyosaarch64_la_LDFLAGS = -Xcompiler -nostdlib

if USE_LIBTOOL
noinst_LTLIBRARIES = liblyosaarch64.la
liblyosaarch64_la_SOURCES = $(LIB_SOURCES)
noinst_DATA = objectlist.awk.in
else
noinst_LIBRARIES = lib.a

lib_a_SOURCES = $(LIB_SOURCES)
lib_a_LIBADD =
lib_a_DEPENDENCIES =
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)
noinst_DATA =
endif

all-local: crt0.o crti.o crtn.o

include $(srcdir)/../../../../../Makefile.shared

ACLOCAL_AMFLAGS = -I ../../../../.. -I ../../../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
