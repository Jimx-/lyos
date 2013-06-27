##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= _misc.o

LIB		= misc.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/misc/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

_misc.o: misc.c
	@echo -e '\tCC\tlib/misc/$@'
	@$(CC) $(CFLAGS) -o $@ $<


