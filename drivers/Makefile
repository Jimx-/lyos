#
# Makefile for Lyos driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

DRVOBJS		= blk_dev/blk_dev.o chr_dev/chr_dev.o net/inet.o pci/pci.o driver.o
DRIVER		= drivers.o

everything : reclean $(DRIVER)

reclean :
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

driver.o: driver.c
	@echo -e '\tCC\t$@'
	@$(CC) $(CFLAGS) -o $@ $<
