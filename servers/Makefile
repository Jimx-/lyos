#
# Makefile for Lyos servers
#

MM 			= mm/mm
INIT		= init/init
DEVMAN		= devman/devman
SERVMAN 	= servman/servman
VFS 		= vfs/vfs
PM			= pm/pm
SYSTASK		= systask/systask

.PHONY: everything realclean
	
everything : realclean $(MM) $(INIT) $(DEVMAN) $(PM) $(VFS) $(SYSTASK) $(SERVMAN)

realclean :
	@rm -f $(MM) $(INIT) $(DEVMAN) $(PM) $(VFS) $(SYSTASK)	$(SERVMAN)

$(MM):
	@(cd mm; make)
	@cp -f $(MM) $(DESTDIR)/sbin

$(VFS):
	@(cd vfs; make)
	@cp -f $(VFS) $(DESTDIR)/sbin

$(INIT):
	@(cd init; make)
	@cp -f $(INIT) $(DESTDIR)/sbin

$(DEVMAN):
	@(cd devman; make)
	@cp -f $(DEVMAN) $(DESTDIR)/sbin

$(PM):
	@(cd pm; make)
	@cp -f $(PM) $(DESTDIR)/sbin

$(SYSTASK):
	@(cd systask; make)
	@cp -f $(SYSTASK) $(DESTDIR)/sbin

$(SERVMAN):
	@(cd servman; make)
	@cp -f $(SERVMAN) $(DESTDIR)/sbin
