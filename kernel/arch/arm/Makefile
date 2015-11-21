
KERNELOBJS	= head.o kernel.o start.o system.o memory.o fpu.o direct_tty.o clock.o \
				page.o kliba.o protect.o bootmods/bootmods.o

ARCHKERNEL		= arch-kernel.o

# Import configuration
ifeq ($(wildcard $(SRCDIR)/.config),) 
else
	include $(SRCDIR)/.config
endif

everything : realclean $(ARCHKERNEL)

realclean :
	@rm -f $(KERNELOBJS) $(machine-y)

machine-$(CONFIG_ARCH_OMAP2PLUS) += mach-omap2/mach-omap2.o

$(ARCHKERNEL): $(KERNELOBJS) $(machine-y)
	@echo -e '\tLD\tkernel/arch/arm/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS) $(machine-y)

%.o : %.S
	@echo -e '\tAS\tkernel/arch/arm/$@'
	@$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/arm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

mach-omap2/mach-omap2.o:
	@(cd mach-omap2; make)

bootmods/bootmods.o:
	@(cd bootmods; make)
