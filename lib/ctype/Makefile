##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
ARFLAGS		= rcs

LOBJS		= isalnum.o isalpha.o iscntrl.o isdigit.o \
			isgraph.o islower.o isprint.o ispunct.o \
			isspace.o isupper.o isxdigit.o \
			tolower.o toupper.o

LIB		= ctype.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LIB) $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/ctype/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/ctype/$@'
	@$(CC) $(CFLAGS) -o $@ $<

