#
# Makefile for Lyos servers
#

SERVOBJS	= mm/mm.o servman/servman.o vfs/vfs.o
INIT		= init/init
DEVMAN		= devman/devman
PM			= pm/pm
SERVERS		= servers.o

.PHONY: everything realclean
	
everything : realclean $(SERVERS) $(INIT) $(DEVMAN) $(PM)

realclean :
	@rm -f $(SERVERS) $(SERVOBJS) $(INIT) $(DEVMAN) $(PM)

$(SERVERS): $(SERVOBJS)
	@echo -e '\tLD\tservers/$@'
	@$(LD) -r -o $(SERVERS) $(SERVOBJS)

mm/mm.o:
	@(cd mm; make)

servman/servman.o:
	@(cd servman; make)

vfs/vfs.o:
	@(cd vfs; make)

$(INIT):
	@(cd init; make)
	@cp -f $(INIT) $(DESTDIR)/sbin

$(DEVMAN):
	@(cd devman; make)
	@cp -f $(DEVMAN) $(DESTDIR)/sbin

$(PM):
	@(cd pm; make)
	@cp -f $(PM) $(DESTDIR)/sbin
