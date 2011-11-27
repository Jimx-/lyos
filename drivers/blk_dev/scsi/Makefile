#
# Makefile for scsi driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../../include/ -I ../../..include/sys/ -f elf
CFLAGS		= -I ../../../include/ -I ../../../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

SCSI		= scsi.o

everything : reclean $(SCSI)

reclean :
	rm -f $(SCSI)

$(SCSI): scsi.c
	$(CC) $(CFLAGS) -o $@ $<


