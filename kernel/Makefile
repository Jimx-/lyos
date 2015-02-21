##############################
# Makefile for Lyos kernel.  #
##############################

AR		= ar
ARFLAGS		= rcs

LDSCRIPT = arch/$(ARCH)/lyos.lds
LDFLAGS		= -T $(LDSCRIPT) -Map $(ARCHDIR)/System.map

KERNELOBJS	= arch/$(ARCH)/arch-kernel.o main.o\
			clock.o global.o proc.o \
			klib.o cpulocals.o \
			sysinfo.o system/system.o interrupt.o system.o smp.o \
			clocksource.o
LIB			= $(LIBOUTDIR)/libexec.a $(LIBOUTDIR)/liblyos.a
LIBC		= $(DESTDIR)/usr/lib/libc.a 

KRNL		= $(ARCHDIR)/lyos.bin

everything : realclean $(KRNL)

realclean :
	@rm -f $(KRNL) $(KERNELOBJS)

$(KRNL): $(KERNELOBJS) $(LIB) $(LIBC)
	@echo -e '\tLD\t$@'
	@$(LD) $(LDFLAGS) -o $(KRNL) $^
	@cp -f $(KRNL) $(DESTDIR)/boot/

arch/$(ARCH)/arch-kernel.o:
	@(cd arch/$(ARCH); make)

%.o: %.c
	@echo -e '\tCC\tkernel/$@'
	@$(CC) $(CFLAGS) -o $@ $<

system/system.o:
	@(cd system; make)

