KERNELOBJS	= head.o kernel.o system.o memory.o start.o smp.o direct_tty.o fpu.o \
			setup_cpulocals.o clock.o kliba.o page.o irq.o watchdog.o

ARCHKERNEL		= arch-kernel.o
LINKERSCRIPT	= lyos.lds

everything : realclean $(ARCHKERNEL) .linker_script

realclean :
	@rm -f $(KERNELOBJS) $(LINKERSCRIPT)

$(ARCHKERNEL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/arch/aarch64/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS)

.linker_script : lyos.lds.S
	@$(CPP) $(ASFLAGS) -E -P -D__ASSEMBLY__ -o $(LINKERSCRIPT) $<

%.o : %.S
	@echo -e '\tAS\tkernel/arch/aarch64/$@'
	@$(CC) $(CFLAGS) -c -D__ASSEMBLY__ -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/aarch64/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
