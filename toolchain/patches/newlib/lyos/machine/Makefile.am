AUTOMAKE_OPTIONS = cygnus

SUBDIRS = $(machine_dir) .

if HAVE_MACHINE_DIR
MACHLIB = lib.a
endif

noinst_DATA = crt0.o crti.o crtn.o

if USE_LIBTOOL
else
noinst_LIBRARIES = $(MACHLIB)

lib.a: $(machine_dir)/lib.a
	rm -f $@
	ln $(machine_dir)/lib.a $@ >/dev/null 2>/dev/null || \
	 cp $(machine_dir)/lib.a $@
endif

crt0.o: $(machine_dir)/crt0.o
	rm -f $@
	ln $(machine_dir)/crt0.o $@ >/dev/null 2>/dev/null \
	 || cp $(machine_dir)/crt0.o $@

crti.o: $(machine_dir)/crti.o
	rm -f $@
	ln $(machine_dir)/crti.o $@ >/dev/null 2>/dev/null \
	 || cp $(machine_dir)/crti.o $@

crtn.o: $(machine_dir)/crtn.o
	rm -f $@
	ln $(machine_dir)/crtn.o $@ >/dev/null 2>/dev/null \
	 || cp $(machine_dir)/crtn.o $@

$(machine_dir)/crt0.o: ; @true
$(machine_dir)/crti.o: ; @true
$(machine_dir)/crtn.o: ; @true

CLEANFILES = crt0.o crti.o crtn.o

doc:

ACLOCAL_AMFLAGS = -I ../../../.. -I ../../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
