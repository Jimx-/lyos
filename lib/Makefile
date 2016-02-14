#
# Makefile for Lyos libraries.

LIBBDEV 	= $(LIBOUTDIR)/libbdev.a
LIBEXEC 	= $(LIBOUTDIR)/libexec.a
LIBLYOS 	= $(LIBOUTDIR)/liblyos.a
LIBSYSFS	= $(LIBOUTDIR)/libsysfs.a
LIBFSDRIVER	= $(LIBOUTDIR)/libfsdriver.a
LIBINPUTDRV	= $(LIBOUTDIR)/libinputdriver.a
LIBBLOCKDRV = $(LIBOUTDIR)/libblockdriver.a
LIBCHARDRV = $(LIBOUTDIR)/libchardriver.a
LIBMEMFS	= $(LIBOUTDIR)/libmemfs.a
LIBDEVMAN	= $(LIBOUTDIR)/libdevman.a
LIBPTHREAD	= $(LIBOUTDIR)/libpthread.a

.PHONY: everything realclean
	
everything : realclean $(LIBSYSFS) $(LIBBDEV) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER)\
				$(LIBMEMFS) $(LIBINPUTDRV) $(LIBDEVMAN) $(LIBPTHREAD) $(LIBBLOCKDRV) $(LIBCHARDRV)

realclean :
	@rm -f $(LIBSYSFS) $(LIBBDEV) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER) $(LIBMEMFS) \
		$(LIBINPUTDRV) $(LIBDEVMAN) $(LIBPTHREAD) $(LIBBLOCKDRV) $(LIBCHARDRV)

$(LIBBDEV): 
	@(cd libbdev; make)

$(LIBEXEC): 
	@(cd libexec; make)

$(LIBLYOS): 
	@(cd liblyos; make)

$(LIBSYSFS): 
	@(cd libsysfs; make)

$(LIBFSDRIVER):
	@(cd libfsdriver; make)
	
$(LIBINPUTDRV):
	@(cd libinputdriver; make)

$(LIBBLOCKDRV):
	@(cd libblockdriver; make)

$(LIBCHARDRV):
	@(cd libchardriver; make)

$(LIBMEMFS):
	@(cd libmemfs; make)

$(LIBDEVMAN):
	@(cd libdevman; make)

$(LIBPTHREAD):
	@(cd libpthread; make)
