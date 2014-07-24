##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
ARFLAGS		= rcs

LOBJS		= ../../arch/$(ARCH)/lib/$(ARCH)-lib.o \
			printl.o spin.o time.o data_copy.o procctl.o mmap.o

LIB		= liblyos.a

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tAR\tlib/liblyos/$@'
	@$(AR) $(ARFLAGS) $@ $^

../../arch/$(ARCH)/lib/$(ARCH)-lib.o:
	@(cd ../../arch/$(ARCH)/lib; make)
 
%.o: %.c
	@echo -e '\tCC\tlib/liblyos/$@'
	@$(CC) $(CFLAGS) -o $@ $<



