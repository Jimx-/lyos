##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= ../arch/$(ARCH)/lib/$(ARCH)-lib.o \
			printl.o spin.o 

LIB		= liblyos.a

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tAR\tlib/$@'
	@$(AR) $(ARFLAGS) $@ $^

../arch/$(ARCH)/lib/$(ARCH)-lib.o:
	@(cd ../arch/$(ARCH)/lib; make)
 
%.o: %.c
	@echo -e '\tCC\tlib/$@'
	@$(CC) $(CFLAGS) -o $@ $<



