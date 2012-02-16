#
# Makefile for inet driver.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

INET		= inet.o

everything : reclean $(INET)

reclean :
	rm -f $(INET)

$(INET): inet.c
	$(CC) $(CFLAGS) -o $@ $<


