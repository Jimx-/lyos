##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

OBJ		= assert.o

everything : realclean $(OBJ)

realclean :
	@rm -f $(OBJ)

assert.o: assert.c
	@echo -e '\tCC\tlib/assert/$@'
	@$(CC) $(CFLAGS) -o $@ $<


