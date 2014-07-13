#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBS 		= $(LIBDIR)/libdriver/libdriver.a
DRVOBJS		= block/block.o char/char.o net/inet.o pci/pci.o devman/devman.o \
				firmware/firmware.o $(LIBS)
DRIVER		= drivers.o

everything : realclean $(DRIVER)

realclean :
	@rm -f $(DRVOBJS)

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

devman/devman.o: $(LIBS)
	@(cd devman; make)

firmware/firmware.o: $(LIBS)
	@(cd firmware; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)
