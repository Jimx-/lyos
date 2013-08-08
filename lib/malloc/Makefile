##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
ARFLAGS		= rcs

OBJ		= malloc.o

everything : realclean $(OBJ)

realclean :
	@rm -f $(OBJ)

malloc.o: malloc.c
	@echo -e '\tCC\tlib/malloc/$@'
	@$(CC) $(CFLAGS) -o $@ $<


