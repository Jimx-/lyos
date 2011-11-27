##############################
# Makefile for Lyos kernel.  #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

KERNELOBJS	= ../arch/$(ARCH)/kernel/$(ARCH)_krnl.o start.o main.o\
			clock.o \
			i8259.o global.o protect.o proc.o\
			systask.o\
			klib.o sys.o

KRNL		= krnl.o

everything : reclean $(KRNL)

reclean :
	rm -f $(KERNELOBJS)

$(KRNL): $(KERNELOBJS)
	$(LD) -r -o $(KRNL) $(KERNELOBJS)

../arch/$(ARCH)/kernel/$(ARCH)_krnl.o:
	(cd ../arch/$(ARCH)/kernel; make)

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
