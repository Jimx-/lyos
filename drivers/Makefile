#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBS 		= libdriver/libdriver.a
DRVOBJS		= block/block.o char/char.o net/inet.o pci/pci.o \
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

firmware/firmware.o: $(LIBS)
	@(cd firmware; make)

libdriver/libdriver.a:
	@(cd libdriver; make)
