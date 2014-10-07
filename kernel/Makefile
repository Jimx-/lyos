##############################
# Makefile for Lyos kernel.  #
##############################

AR		= ar
ARFLAGS		= rcs

KERNELOBJS	= arch/$(ARCH)/arch-kernel.o main.o\
			clock.o global.o proc.o systask.o\
			klib.o cpulocals.o log.o vmctl.o

KRNL		= krnl.o

everything : realclean $(KRNL)

realclean :
	@rm -f $(KERNELOBJS)

$(KRNL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/$@'
	@$(LD) -r -o $(KRNL) $(KERNELOBJS)

arch/$(ARCH)/arch-kernel.o:
	@(cd arch/$(ARCH); make)

%.o: %.c
	@echo -e '\tCC\tkernel/$@'
	@$(CC) $(CFLAGS) -o $@ $<

