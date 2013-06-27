##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

OBJ		= malloc.o

everything : realclean $(OBJ)

realclean :
	@rm -f $(OBJ)

malloc.o: malloc.c
	@echo -e '\tCC\tlib/malloc/$@'
	@$(CC) $(CFLAGS) -o $@ $<


