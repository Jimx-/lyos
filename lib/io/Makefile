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

LOBJS		= open.o read.o write.o close.o unlink.o\
			lseek.o stat.o

LIB		= io.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

open.o: open.c
	$(CC) $(CFLAGS) -o $@ $<

read.o: read.c
	$(CC) $(CFLAGS) -o $@ $<

write.o: write.c
	$(CC) $(CFLAGS) -o $@ $<

close.o: close.c
	$(CC) $(CFLAGS) -o $@ $<

unlink.o: unlink.c
	$(CC) $(CFLAGS) -o $@ $<

stat.o: stat.c
	$(CC) $(CFLAGS) -o $@ $<

lseek.o: lseek.c
	$(CC) $(CFLAGS) -o $@ $<


