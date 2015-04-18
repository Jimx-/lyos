#
# Makefile for Lyos libraries.

LIBDRIVER 	= $(LIBOUTDIR)/libdriver.a
LIBEXEC 	= $(LIBOUTDIR)/libexec.a
LIBLYOS 	= $(LIBOUTDIR)/liblyos.a
LIBSYSFS	= $(LIBOUTDIR)/libsysfs.a
LIBFSDRIVER	= $(LIBOUTDIR)/libfsdriver.a
LIBMEMFS	= $(LIBOUTDIR)/libmemfs.a

.PHONY: everything realclean
	
everything : realclean $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER)\
				$(LIBMEMFS)

realclean :
	@rm -f $(LIBSYSFS) $(LIBDRIVER) $(LIBLYOS) $(LIBEXEC) $(LIBFSDRIVER) $(LIBMEMFS) \

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
	
$(LIBMEMFS):
	@(cd libmemfs; make)
