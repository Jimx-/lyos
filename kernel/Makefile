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
LIBC		= $(DESTDIR)/usr/lib/libc.a

ifeq ($(CONFIG_OF),y)
	LIB += $(LIBOUTDIR)/libof.a $(LIBOUTDIR)/libfdt.a
endif

KRNL		= $(ARCHDIR)/lyos.elf

everything : realclean $(KRNL)

realclean :
	@rm -f $(KRNL) $(KERNELOBJS)

$(KRNL): $(KERNELOBJS) $(LIB) $(LIBC)
	@echo -e '\tLD\t$@'
	@$(LD) $(LDFLAGS) -o $(KRNL) $^
	@cp -f $(KRNL) $(KRNLBIN) $(DESTDIR)/boot/

arch/$(ARCH)/arch-kernel.o:
	@(cd arch/$(ARCH); make)

%.o: %.c
	@echo -e '\tCC\tkernel/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<

system/system.o:
	@(cd system; make)
