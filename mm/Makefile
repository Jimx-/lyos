#
# Makefile for the Lyos memory manager.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

MMOBJS 		= main.o forkexit.o exec.o page.o memory.o signal.o
MM		= mm.o

everything : reclean $(MM)

reclean :
	rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	$(LD) -r -o $(MM) $(MMOBJS)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

forkexit.o: forkexit.c
	$(CC) $(CFLAGS) -o $@ $<

exec.o: exec.c
	$(CC) $(CFLAGS) -o $@ $<

page.o: page.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

memory.o: memory.c
	$(CC) $(CFLAGS) -o $@ $<

signal.o: signal.c
	$(CC) $(CFLAGS) -o $@ $<
