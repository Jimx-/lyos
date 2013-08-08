##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
ARFLAGS		= rcs

LOBJS		= open.o read.o write.o close.o unlink.o\
			lseek.o stat.o

LIB		= io.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/io/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/io/$@'
	@$(CC) $(CFLAGS) -o $@ $<

