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
	$(Q)$(MAKE) -C libbdev $(MAKEFLAGS)
	$(Q)$(MAKE) -C libbdev $(MAKEFLAGS) install

$(LIBEXEC): 
	$(Q)$(MAKE) -C libexec $(MAKEFLAGS)
	$(Q)$(MAKE) -C libexec $(MAKEFLAGS) install

$(LIBLYOS): 
	@(cd liblyos; make)

$(LIBSYSFS): 
	$(Q)$(MAKE) -C libsysfs $(MAKEFLAGS)
	$(Q)$(MAKE) -C libsysfs $(MAKEFLAGS) install

$(LIBFSDRIVER):
	$(Q)$(MAKE) -C libfsdriver $(MAKEFLAGS)
	$(Q)$(MAKE) -C libfsdriver $(MAKEFLAGS) install
	
$(LIBINPUTDRV):
	$(Q)$(MAKE) -C libinputdriver $(MAKEFLAGS)
	$(Q)$(MAKE) -C libinputdriver $(MAKEFLAGS) install

$(LIBBLOCKDRV):
	$(Q)$(MAKE) -C libblockdriver $(MAKEFLAGS)
	$(Q)$(MAKE) -C libblockdriver $(MAKEFLAGS) install

$(LIBCHARDRV):
	$(Q)$(MAKE) -C libchardriver $(MAKEFLAGS)
	$(Q)$(MAKE) -C libchardriver $(MAKEFLAGS) install

$(LIBMEMFS):
	$(Q)$(MAKE) -C libmemfs $(MAKEFLAGS)
	$(Q)$(MAKE) -C libmemfs $(MAKEFLAGS) install

$(LIBDEVMAN):
	$(Q)$(MAKE) -C libdevman $(MAKEFLAGS)
	$(Q)$(MAKE) -C libdevman $(MAKEFLAGS) install

$(LIBPTHREAD):
	$(Q)$(MAKE) -C libpthread $(MAKEFLAGS)
	$(Q)$(MAKE) -C libpthread $(MAKEFLAGS) install
