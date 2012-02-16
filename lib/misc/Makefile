##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= _misc.o

LIB		= misc.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

_misc.o: misc.c
	$(CC) $(CFLAGS) -o $@ $<


