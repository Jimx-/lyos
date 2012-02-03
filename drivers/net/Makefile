#
# Makefile for inet driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -I ../../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

INET		= inet.o

everything : reclean $(INET)

reclean :
	rm -f $(INET)

$(INET): inet.c
	$(CC) $(CFLAGS) -o $@ $<


