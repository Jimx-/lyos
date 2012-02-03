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

OBJ		= malloc.o

everything : reclean $(OBJ)

reclean :
	rm -f $(OBJ)

malloc.o: malloc.c
	$(CC) $(CFLAGS) -o $@ $<


