##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= memcmp.o strcmp.o strcat.o \
			memcpy.o strlen.o strcpy.o

LIB		= string.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/string/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/string/$@'
	@$(CC) $(CFLAGS) -o $@ $<

