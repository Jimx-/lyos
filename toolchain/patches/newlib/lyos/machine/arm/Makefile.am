AUTOMAKE_OPTIONS = cygnus

noinst_LIBRARIES = lib.a

lib_a_SOURCES = gate_intr.S __sigreturn.S __udivsi3.c __umodsi3.c __clzsi2.c adddf3.c eqdf2.c ledf2.c gedf2.c muldf3.c divdf3.c subdf3.c floatsidf.c floatunsidf.c fixdfsi.c __divsi3.c __modsi3.c clone.c
lib_a_LIBADD =
lib_a_DEPENDENCIES =
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)

if MAY_SUPPLY_SYSCALLS
all-local: crt0.o crti.o crtn.o
endif

ACLOCAL_AMFLAGS = -I ../../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
