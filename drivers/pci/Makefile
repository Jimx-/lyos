#
# Makefile for pci driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -I ../../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

PCI		= pci.o

everything : reclean $(PCI)

reclean :
	rm -f $(PCI)

$(PCI): pci.c
	$(CC) $(CFLAGS) -o $@ $<


