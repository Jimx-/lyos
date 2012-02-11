##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -f elf
CFLAGS		= -I ../../include/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= getpid.o getuid.o \
			fork.o wait.o exec.o kill.o \
			uname.o

LIB		= posix.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

getpid.o: getpid.c
	$(CC) $(CFLAGS) -o $@ $<
	
getuid.o: getuid.c
	$(CC) $(CFLAGS) -o $@ $<

fork.o: fork.c
	$(CC) $(CFLAGS) -o $@ $<

wait.o: wait.c
	$(CC) $(CFLAGS) -o $@ $<

exec.o: exec.c
	$(CC) $(CFLAGS) -o $@ $<

kill.o: kill.c
	$(CC) $(CFLAGS) -o $@ $<

uname.o: uname.c
	$(CC) $(CFLAGS) -o $@ $<


