##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -I ../../include/lyos/ -I ../include/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/lyos/ -I ../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= isalnum.o isalpha.o iscntrl.o isdigit.o \
			isgraph.o islower.o isprint.o ispunct.o \
			isspace.o isupper.o isxdigit.o \
			tolower.o toupper.o

LIB		= ctype.o

everything : realclean $(LIB)

realclean :
	rm -f $(LIB) $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

isalnum.o: isalnum.c
	 $(CC) $(CFLAGS) -o $@ $<

isalpha.o: isalpha.c
	$(CC) $(CFLAGS) -o $@ $<

iscntrl.o: iscntrl.c
	$(CC) $(CFLAGS) -o $@ $<

isdigit.o: isdigit.c
	$(CC) $(CFLAGS) -o $@ $<

isgraph.o: isgraph.c
	$(CC) $(CFLAGS) -o $@ $<

islower.o: islower.c
	$(CC) $(CFLAGS) -o $@ $<

isprint.o: isprint.c
	$(CC) $(CFLAGS) -o $@ $<

ispunct.o: ispunct.c 
	$(CC) $(CFLAGS) -o $@ $<

isspace.o: isspace.c
	$(CC) $(CFLAGS) -o $@ $<

isupper.o: isupper.c
	$(CC) $(CFLAGS) -o $@ $<

isxdigit.o: isxdigit.c
	$(CC) $(CFLAGS) -o $@ $<

tolower.o: tolower.c
	$(CC) $(CFLAGS) -o $@ $<

toupper.o: toupper.c
	$(CC) $(CFLAGS) -o $@ $<




