#
# Makefile for Lyos servers
#

SERVOBJS	= mm/mm.o servman/servman.o vfs/vfs.o
INIT		= init/init
DEVMAN		= devman/devman
SERVERS		= servers.o

.PHONY: everything realclean
	
everything : realclean $(SERVERS) $(INIT) $(DEVMAN)

realclean :
	@rm -f $(SERVERS) $(SERVOBJS) $(INIT) $(DEVMAN)

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
