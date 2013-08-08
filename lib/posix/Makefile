##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
ARFLAGS		= rcs

LOBJS		= getpid.o getuid.o \
			fork.o wait.o exec.o kill.o \
			uname.o

LIB		= posix.o

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tLD\tlib/posix/$@'
	@$(LD) -r -o $(LIB) $(LOBJS)

%.o: %.c
	@echo -e '\tCC\tlib/posix/$@'
	@$(CC) $(CFLAGS) -o $@ $<
	
