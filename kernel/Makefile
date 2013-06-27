##############################
# Makefile for Lyos kernel.  #
##############################

AR		= ar
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINC)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
ARFLAGS		= rcs

KERNELOBJS	= ../arch/$(ARCH)/kernel/$(ARCH)-krnl.o main.o\
			clock.o \
			global.o proc.o\
			systask.o\
			klib.o sys.o

KRNL		= krnl.o

everything : realclean $(KRNL)

realclean :
	@rm -f $(KERNELOBJS)

$(KRNL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/$@'
	@$(LD) -r -o $(KRNL) $(KERNELOBJS)

../arch/$(ARCH)/kernel/$(ARCH)-krnl.o:
	@(cd ../arch/$(ARCH)/kernel; make)

%.o: %.c
	@echo -e '\tCC\tkernel/$@'
	@$(CC) $(CFLAGS) -o $@ $<

