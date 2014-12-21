#
# Makefile for Lyos servers
#

SERVOBJS	= mm/mm.o servman/servman.o
INIT		= init/init
DEVMAN		= devman/devman
VFS 		= vfs/vfs
PM			= pm/pm
SERVERS		= servers.o

.PHONY: everything realclean
	
everything : realclean $(SERVERS) $(INIT) $(DEVMAN) $(PM) $(VFS)

realclean :
	@rm -f $(SERVERS) $(SERVOBJS) $(INIT) $(DEVMAN) $(PM) $(VFS)

$(SERVERS): $(SERVOBJS)
	@echo -e '\tLD\tservers/$@'
	@$(LD) -r -o $(SERVERS) $(SERVOBJS)

mm/mm.o:
	@(cd mm; make)

servman/servman.o:
	@(cd servman; make)

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
