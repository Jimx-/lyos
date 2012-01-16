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

OBJ		= assert.o

everything : realclean $(OBJ)

realclean :
	rm -f $(OBJ)

assert.o: assert.c
	$(CC) $(CFLAGS) -o $@ $<


