##############################
# Makefile for Lyos library. #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

LOBJS		= syscall.o\
			printf.o vsprintf.o\
			string.o misc.o\
			open.o read.o write.o close.o unlink.o\
			lseek.o\
			getpid.o getuid.o stat.o\
			fork.o exit.o wait.o exec.o kill.o \
			uname.o

LIB		= lyoscrt.a

everything : reclean $(LIB)

reclean :
	rm -f $(LOBJS)

$(LIB): $(LOBJS)
	$(AR) $(ARFLAGS) $@ $^

syscall.o : syscall.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

printf.o: printf.c
	$(CC) $(CFLAGS) -o $@ $<

vsprintf.o: vsprintf.c
	$(CC) $(CFLAGS) -o $@ $<

misc.o: misc.c
	$(CC) $(CFLAGS) -o $@ $<

string.o : string.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

open.o: open.c
	$(CC) $(CFLAGS) -o $@ $<

read.o: read.c
	$(CC) $(CFLAGS) -o $@ $<

write.o: write.c
	$(CC) $(CFLAGS) -o $@ $<

close.o: close.c
	$(CC) $(CFLAGS) -o $@ $<

unlink.o: unlink.c
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

stat.o: stat.c
	$(CC) $(CFLAGS) -o $@ $<

lseek.o: lseek.c
	$(CC) $(CFLAGS) -o $@ $<

kill.o: kill.c
	$(CC) $(CFLAGS) -o $@ $<

uname.o: uname.c
	$(CC) $(CFLAGS) -o $@ $<


