#
# Makefile for the Lyos character device drivers.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -I ../../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

CHRDRVOBJS	= console.o keyboard.o tty.o
CHRDRV		= chr_dev.o

everything : reclean $(CHRDRV)

reclean :
	rm -f $(CHRDRVOBJS)

$(CHRDRV): $(CHRDRVOBJS)
	$(LD) -r -o $(CHRDRV) $(CHRDRVOBJS)

console.o: console.c
	$(CC) $(CFLAGS) -o $@ $<

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) -o $@ $<

tty.o: tty.c
	$(CC) $(CFLAGS) -o $@ $<
