#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

DRVOBJS		= blk_dev/blk_dev.o chr_dev/chr_dev.o net/inet.o pci/pci.o \
				firmware/firmware.o driver.o
DRIVER		= drivers.o

everything : realclean $(DRIVER)

realclean :
	@rm -f $(DRVOBJS)

$(DRIVER): $(DRVOBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $(DRIVER) $(DRVOBJS)

blk_dev/blk_dev.o:
	@(cd blk_dev; make)

chr_dev/chr_dev.o:
	@(cd chr_dev; make)

net/inet.o:
	@(cd net; make)

pci/pci.o:
	@(cd pci; make)

firmware/firmware.o:
	@(cd firmware; make)

driver.o: driver.c
	@echo -e '\tCC\t$@'
	@$(CC) $(CFLAGS) -o $@ $<
