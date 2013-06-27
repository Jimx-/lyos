##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ASMKFLAGS	= -I ../include/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= ../arch/$(ARCH)/lib/$(ARCH)-lib.o \
			assert/assert.o \
			ctype/ctype.o \
			io/io.o \
			malloc/malloc.o \
			misc/misc.o \
			posix/posix.o \
			stdio/stdio.o \
			stdlib/stdlib.o \
			string/string.o

LIB		= liblyos.a

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tAR\tlib/$@'
	@$(AR) $(ARFLAGS) $@ $^

../arch/$(ARCH)/lib/$(ARCH)-lib.o:
	@(cd ../arch/$(ARCH)/lib; make)
 
assert/assert.o: 
	@(cd assert; make)

ctype/ctype.o:
	@(cd ctype; make)

stdio/stdio.o:
	@(cd stdio; make)

stdlib/stdlib.o:
	@(cd stdlib; make)

io/io.o:
	@(cd io; make)

malloc/malloc.o:
	@(cd malloc; make)

misc/misc.o:
	@(cd misc; make)

posix/posix.o:
	@(cd posix; make)

string/string.o:
	@(cd string; make)


