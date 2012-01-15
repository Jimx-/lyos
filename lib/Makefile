##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -I include/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -I include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= ../arch/$(ARCH)/lib/$(ARCH)_lib.o \
			assert/assert.o \
			ctype/ctype.o \
			io/io.o \
			posix/posix.o \
			stdio/stdio.o \
			string/string.o

LIB		= liblyos.a

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(AR) $(ARFLAGS) $@ $^

../arch/$(ARCH)/lib/$(ARCH)_lib.o:
	(cd ../arch/$(ARCH)/lib; make)
 
assert/assert.o: 
	(cd assert; make)

ctype/ctype.o:
	(cd ctype; make)

stdio/stdio.o:
	(cd stdio; make)

io/io.o:
	(cd io; make)

posix/posix.o:
	(cd posix; make)

string/string.o:
	(cd string; make)


