#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBS 		= $(LIBDIR)/libdriver/libdriver.a
DRVOBJS		= block/block.o char/char.o pci/pci.o \
				firmware/firmware.o $(LIBS)
DEVMAN 		= devman/devman
INET 		= inet/inet
DRIVER		= drivers.o

everything : realclean $(DRIVER) $(DEVMAN) $(INET)

realclean :
	@rm -f $(DRVOBJS) $(DEVMAN) $(INET)

$(DRIVER): $(DRVOBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $(DRIVER) $(DRVOBJS)

block/block.o: $(LIBS)
	@(cd block; make)

char/char.o: $(LIBS)
	@(cd char; make)

net/inet.o: $(LIBS)
	@(cd net; make)

pci/pci.o: $(LIBS)
	@(cd pci; make)

$(DEVMAN):
	@(cd devman; make)
	@(cp -f $(DEVMAN) ../sysroot/sbin/)

$(INET):
	@(cd inet; make)
	@(cp -f $(INET) ../sysroot/usr/sbin/)

firmware/firmware.o: $(LIBS)
	@(cd firmware; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)
