KERNELOBJS	= head.o kernel.o start.o system.o memory.o fpu.o direct_tty.o clock.o \
				page.o kliba.o protect.o smp.o setup_cpulocals.o watchdog.o irq.o \
				vsprintf.o bootmods/bootmods.o

ARCHKERNEL		= arch-kernel.o
LINKERSCRIPT	= lyos.lds

everything : realclean $(ARCHKERNEL) .linker_script

realclean :
	@rm -f $(KERNELOBJS) $(LINKERSCRIPT)

$(ARCHKERNEL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/arch/riscv/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS)

.linker_script : lyos.lds.S
	@$(CPP) $(ASFLAGS) -E -P -D__ASSEMBLY__ -o $(LINKERSCRIPT) $<

%.o : %.S
	@echo -e '\tAS\tkernel/arch/riscv/$@'
	@$(CC) $(CFLAGS) -c -D__ASSEMBLY__ -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/riscv/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<

bootmods/bootmods.o:
	@(cd bootmods; make)
