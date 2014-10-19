#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBS 		= $(LIBDIR)/libdriver/libdriver.a
DRVOBJS		= block/block.o char/char.o \
				firmware/firmware.o $(LIBS)
DEVMAN 		= devman/devman
INET 		= inet/inet
PCI 		= pci/pci
DRIVER		= drivers.o

everything : realclean $(DRIVER) $(DEVMAN) $(INET) $(PCI)

realclean :
	@rm -f $(DRIVER) $(DRVOBJS) $(DEVMAN) $(INET) $(PCI)

$(DRIVER): $(DRVOBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $(DRIVER) $(DRVOBJS)

block/block.o: $(LIBS)
	@(cd block; make)

char/char.o: $(LIBS)
	@(cd char; make)

net/inet.o: $(LIBS)
	@(cd net; make)

$(PCI):
	@(cd pci; make)
	@(cp -f $(PCI) ../sysroot/sbin/)

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
