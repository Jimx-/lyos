#
# Makefile for Lyos libraries.

LIBDRIVER 	= $(LIBOUTDIR)/libdriver.a
LIBEXEC 	= $(LIBOUTDIR)/libexec.a
LIBLYOS 	= $(LIBOUTDIR)/liblyos.a
LIBSYSFS	= $(LIBOUTDIR)/libsysfs.a
LIBFSDRIVER	= $(LIBOUTDIR)/libfsdriver.a
LIBINPUTDRV	= $(LIBOUTDIR)/libinputdriver.a
LIBMEMFS	= $(LIBOUTDIR)/libmemfs.a
LIBDEVMAN	= $(LIBOUTDIR)/libdevman.a
LIBPTHREAD	= $(LIBOUTDIR)/libpthread.a

.PHONY: everything realclean
	
everything : realclean $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER)\
				$(LIBMEMFS) $(LIBINPUTDRV) $(LIBDEVMAN) $(LIBPTHREAD)

realclean :
	@rm -f $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER) $(LIBMEMFS) \
		$(LIBINPUTDRV) $(LIBDEVMAN) $(LIBPTHREAD)

$(LIBDRIVER): 
	@(cd libdriver; make)

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
	
$(LIBMEMFS):
	@(cd libmemfs; make)

$(LIBDEVMAN):
	@(cd libdevman; make)

$(LIBPTHREAD):
	@(cd libpthread; make)
