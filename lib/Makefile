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

.PHONY: everything realclean
	
everything : realclean $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER)\
				$(LIBMEMFS) $(LIBINPUTDRV) $(LIBDEVMAN)

realclean :
	@rm -f $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER) $(LIBMEMFS) \
		$(LIBINPUTDRV) $(LIBDEVMAN)

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
