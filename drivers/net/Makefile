#
# Makefile for inet driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../include/ -I ../..include/lyos/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/lyos/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

INET		= inet.o

everything : reclean $(INET)

reclean :
	rm -f $(INET)

$(INET): inet.c
	$(CC) $(CFLAGS) -o $@ $<


