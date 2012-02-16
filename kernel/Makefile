##############################
# Makefile for Lyos kernel.  #
##############################

ASM		= nasm
CC		= gcc
LD		= ld
AR		= ar
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINC)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINC)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

KERNELOBJS	= ../arch/$(ARCH)/kernel/$(ARCH)-krnl.o main.o\
			clock.o \
			global.o proc.o\
			systask.o\
			klib.o sys.o

KRNL		= krnl.o

everything : reclean $(KRNL)

reclean :
	rm -f $(KERNELOBJS)

$(KRNL): $(KERNELOBJS)
	$(LD) -r -o $(KRNL) $(KERNELOBJS)

../arch/$(ARCH)/kernel/$(ARCH)-krnl.o:
	(cd ../arch/$(ARCH)/kernel; make)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

clock.o: clock.c
	$(CC) $(CFLAGS) -o $@ $<

global.o: global.c
	$(CC) $(CFLAGS) -o $@ $<

proc.o: proc.c
	$(CC) $(CFLAGS) -o $@ $<

systask.o: systask.c
	$(CC) $(CFLAGS) -o $@ $<

klib.o: klib.c
	$(CC) $(CFLAGS) -o $@ $<

sys.o: sys.c
	$(CC) $(CFLAGS) -o $@ $<
