##############################
# Makefile for Lyos kernel.  #
##############################

LDSCRIPT = arch/$(ARCH)/lyos.lds
LDFLAGS	= -T $(LDSCRIPT) -Map $(ARCHDIR)/System.map
CFLAGS		+= -D__kernel__

KERNELOBJS	= arch/$(ARCH)/arch-kernel.o main.o clock.o global.o proc.o klib.o \
				sysinfo.o system/system.o interrupt.o system.o smp.o clocksource.o \
				sched.o profile.o watchdog.o

LIB		= $(LIBOUTDIR)/libexec.a $(LIBOUTDIR)/liblyos.a
KLIBC		= lib/klibc.o

ifeq ($(CONFIG_OF),y)
	LIB += $(LIBOUTDIR)/libof.a $(LIBOUTDIR)/libfdt.a
endif

KRNL		= $(ARCHDIR)/lyos.elf

everything : realclean $(KRNL)

realclean :
	@rm -f $(KRNL) $(KERNELOBJS) $(KLIBC)

$(KRNL): $(KERNELOBJS) $(LIB) $(KLIBC)
	@echo -e '\tLD\t$@'
ifeq ($(SUBARCH),x86_64)
	@$(LD) $(LDFLAGS) -o $(KRNL).64 $^
	@$(OBJCOPY) -I elf64-x86-64 -O elf32-i386 $(KRNL).64 $(KRNL)
	@cp -f $(KRNL).64 $(DESTDIR)/boot/
else
	@$(LD) $(LDFLAGS) -o $(KRNL) $^
endif
	@cp -f $(KRNL) $(DESTDIR)/boot/

arch/$(ARCH)/arch-kernel.o:
	@(cd arch/$(ARCH); make)

%.o: %.c
	@echo -e '\tCC\tkernel/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<

system/system.o:
	@(cd system; make)

$(KLIBC):
	@(cd lib; make)
