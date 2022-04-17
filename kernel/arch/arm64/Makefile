KERNELOBJS	= head.o kernel.o system.o memory.o start.o smp.o direct_tty.o fpu.o \
			setup_cpulocals.o clock.o kliba.o page.o irq.o watchdog.o protect.o \
			gate.o bootmods/bootmods.o

ARCHKERNEL		= arch-kernel.o
LINKERSCRIPT	= lyos.lds

everything : realclean $(ARCHKERNEL) .linker_script

realclean :
	@rm -f $(KERNELOBJS) $(machine-y) $(LINKERSCRIPT)

machine-$(CONFIG_ARCH_BCM) += mach-bcm/mach-bcm.o

$(ARCHKERNEL): $(KERNELOBJS) $(machine-y)
	@echo -e '\tLD\tkernel/arch/arm64/$@'
	@$(LD) -r -o $@ $^

.linker_script : lyos.lds.S
	@$(CPP) $(ASFLAGS) -E -P -D__ASSEMBLY__ -o $(LINKERSCRIPT) $<

%.o : %.S
	@echo -e '\tAS\tkernel/arch/arm64/$@'
	@$(CC) $(CFLAGS) -c -D__ASSEMBLY__ -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/arm64/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<

mach-bcm/mach-bcm.o:
	@(cd mach-bcm; make)

bootmods/bootmods.o:
	@(cd bootmods; make)
