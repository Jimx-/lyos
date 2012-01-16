#
# Makefile for the Lyos filesystem.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/lyos/ -f elf
CFLAGS		= -I ../include/ -I ../include/lyos/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

FSOBJS		= main.o lyos/lyosfs.o
FS		= fs.o

everything : reclean $(FS)

reclean :
	rm -f $(FSOBJS)

$(FS): $(FSOBJS)
	$(LD) -r -o $(FS) $(FSOBJS)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

lyos/lyosfs.o:
	(cd lyos; make)

