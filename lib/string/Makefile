##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -I ../../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= memcmp.o strcmp.o strcat.o \
			memcpy.o strlen.o strcpy.o

LIB		= string.o

everything : realclean $(LIB)

realclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

memcmp.o: memcmp.c
	$(CC) $(CFLAGS) -o $@ $<

strcmp.o: strcmp.c
	$(CC) $(CFLAGS) -o $@ $<

strcat.o: strcat.c
	$(CC) $(CFLAGS) -o $@ $<

memcpy.o: memcpy.c
	$(CC) $(CFLAGS) -o $@ $<

strlen.o: strlen.c
	$(CC) $(CFLAGS) -o $@ $<

strcpy.o: strcpy.c
	$(CC) $(CFLAGS) -o $@ $<
