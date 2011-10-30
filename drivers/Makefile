#
# Makefile for Lyos driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

DRVOBJS		= blk_dev/blk_dev.o chr_dev/chr_dev.o net/inet.o pci/pci.o driver.o
DRIVER		= drivers.o

everything : reclean $(DRIVER)

reclean :
	rm -f $(DRVOBJS)

$(DRIVER): $(DRVOBJS)
	$(LD) -r -o $(DRIVER) $(DRVOBJS)

blk_dev/blk_dev.o:
	(cd blk_dev; make)

chr_dev/chr_dev.o:
	(cd chr_dev; make)

net/inet.o:
	(cd net; make)

pci/pci.o:
	(cd pci; make)

driver.o: driver.c
	$(CC) $(CFLAGS) -o $@ $<
