##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
ARFLAGS		= rcs

LOBJS		= exit.o

LIB		= stdlib.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/stdlib/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/stdlib/$@'
	@$(CC) $(CFLAGS) -o $@ $<


