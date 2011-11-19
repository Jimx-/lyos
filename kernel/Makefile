##############################
# Makefile for Lyos kernel.  #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

KERNELOBJS	= kernel.o start.o main.o\
			clock.o \
			i8259.o global.o protect.o proc.o\
			systask.o\
			kliba.o klib.o sys.o reboot.o

KRNL		= krnl.o

everything : reclean $(KRNL)

reclean :
	rm -f $(KERNELOBJS)

$(KRNL): $(KERNELOBJS)
	$(LD) -r -o $(KRNL) $(KERNELOBJS)

kernel.o : kernel.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

reboot.o : reboot.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

start.o: start.c
	$(CC) $(CFLAGS) -o $@ $<

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

clock.o: clock.c
	$(CC) $(CFLAGS) -o $@ $<

i8259.o: i8259.c
	$(CC) $(CFLAGS) -o $@ $<

global.o: global.c
	$(CC) $(CFLAGS) -o $@ $<

protect.o: protect.c
	$(CC) $(CFLAGS) -o $@ $<

proc.o: proc.c
	$(CC) $(CFLAGS) -o $@ $<

systask.o: systask.c
	$(CC) $(CFLAGS) -o $@ $<

klib.o: klib.c
	$(CC) $(CFLAGS) -o $@ $<

sys.o: sys.c
	$(CC) $(CFLAGS) -o $@ $<

kliba.o : kliba.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

