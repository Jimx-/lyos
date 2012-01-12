##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LIBS		= libc/libc.o

LIB		= liblyos.a

everything : reclean $(LIB)

reclean :
	rm -f $(LIBS)

$(LIB): $(LIBS)
	$(AR) $(ARFLAGS) $@ $^

libc/libc.o:
	(cd libc; make)

