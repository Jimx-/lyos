##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -I ../../include/lyos/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/lyos/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= printf.o vsprintf.o

LIB		= stdio.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

printf.o: printf.c
	$(CC) $(CFLAGS) -o $@ $<

vsprintf.o: vsprintf.c
	$(CC) $(CFLAGS) -o $@ $<

