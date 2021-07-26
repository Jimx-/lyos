AUTOMAKE_OPTIONS = cygnus

noinst_LIBRARIES = lib.a

lib_a_SOURCES = gate_intr.S __sigreturn.S clone.c getpagesize.c __ucontext.S makecontext.c
lib_a_LIBADD =
lib_a_DEPENDENCIES =
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)

if MAY_SUPPLY_SYSCALLS
all-local: crt0.o crti.o crtn.o
endif

ACLOCAL_AMFLAGS = -I ../../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
