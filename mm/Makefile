#
# Makefile for the Lyos memory manager.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

MMOBJS 		= ../arch/$(ARCH)/mm/$(ARCH)-mm.o \
			main.o forkexit.o exec.o memory.o signal.o alloc.o
MM		= mm.o

everything : reclean $(MM)

reclean :
	rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	$(LD) -r -o $(MM) $(MMOBJS)

../arch/$(ARCH)/mm/$(ARCH)-mm.o:
	(cd ../arch/$(ARCH)/mm; make)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

forkexit.o: forkexit.c
	$(CC) $(CFLAGS) -o $@ $<

exec.o: exec.c
	$(CC) $(CFLAGS) -o $@ $<

memory.o: memory.c
	$(CC) $(CFLAGS) -o $@ $<

signal.o: signal.c
	$(CC) $(CFLAGS) -o $@ $<

alloc.o: alloc.c
	$(CC) $(CFLAGS) -o $@ $<
