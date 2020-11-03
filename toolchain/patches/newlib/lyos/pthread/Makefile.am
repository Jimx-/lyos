## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCLUDES = -I$(srcdir)/../include -I$(srcdir)/.. $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)

GENERAL_SOURCES = \
	pthread.c \
	pthread_attr.c \
	pthread_barrier.c \
	pthread_cond.c \
	pthread_create.c \
	pthread_internal.h \
	pthread_join.c \
	pthread_mutex.c \
	pthread_once.c \
	pthread_rwlock.c \
	pthread_specific.c \
	pthread_cleanup.c

# libpthread_la_LDFLAGS = -Xcompiler -nostdlib

if USE_LIBTOOL
noinst_LTLIBRARIES = libpthread.la
libpthread_la_SOURCES = $(GENERAL_SOURCES)
noinst_DATA = objectlist.awk.in
else
noinst_LIBRARIES = lib.a
lib_a_SOURCES = $(GENERAL_SOURCES)
lib_a_CFLAGS = $(AM_CFLAGS)
noinst_DATA =
endif # USE_LIBTOOL

include $(srcdir)/../../../../Makefile.shared
