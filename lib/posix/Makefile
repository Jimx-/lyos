##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../../include/ -I ../../include/sys/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= misc.o\
			getpid.o getuid.o \
			fork.o exit.o wait.o exec.o kill.o \
			uname.o

LIB		= posix.o

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(LD) -r -o $(LIB) $(LOBJS)

misc.o: misc.c
	$(CC) $(CFLAGS) -o $@ $<

getpid.o: getpid.c
	$(CC) $(CFLAGS) -o $@ $<
	
getuid.o: getuid.c
	$(CC) $(CFLAGS) -o $@ $<

fork.o: fork.c
	$(CC) $(CFLAGS) -o $@ $<

exit.o: exit.c
	$(CC) $(CFLAGS) -o $@ $<

wait.o: wait.c
	$(CC) $(CFLAGS) -o $@ $<

exec.o: exec.c
	$(CC) $(CFLAGS) -o $@ $<

kill.o: kill.c
	$(CC) $(CFLAGS) -o $@ $<

uname.o: uname.c
	$(CC) $(CFLAGS) -o $@ $<


