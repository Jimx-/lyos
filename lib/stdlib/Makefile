##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -I ../../include/sys/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= exit.o

LIB		= stdlib.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

exit.o: exit.c
	$(CC) $(CFLAGS) -o $@ $<


