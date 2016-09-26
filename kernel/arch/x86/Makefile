
KERNELOBJS	= kernel.o kliba.o i8259.o start.o protect.o apic.o acpi.o \
				system.o gate.o direct_tty.o page.o memory.o clock.o sys_sportio.o \
				apic_asm.o fpu.o hpet.o tsc.o

# Import configuration
ifeq ($(wildcard $(SRCDIR)/.config),) 
else
	include $(SRCDIR)/.config
endif

ifeq ($(CONFIG_SMP),y)
	KERNELOBJS += smp.o trampoline.o trampoline_32.o
endif

ARCHKERNEL		= arch-kernel.o

everything : realclean $(ARCHKERNEL)

realclean :
	@rm -f $(KERNELOBJS)

$(ARCHKERNEL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/arch/x86/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS)

%.o : %.asm
	@echo -e '\tASM\tkernel/arch/x86/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/x86/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
