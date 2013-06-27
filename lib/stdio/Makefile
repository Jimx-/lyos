##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= printf.o vsprintf.o

LIB		= stdio.o

everything : realclean $(LIB)

realclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/stdio/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/stdio/$@'
	@$(CC) $(CFLAGS) -o $@ $<


