OBJS-$(CONFIG_X86_32) = gate.o

OBJS-$(CONFIG_X86_64) = gate_64.o

KERNELOBJS	= $(OBJS-y) kernel.o kliba.o i8259.o start.o protect.o apic.o acpi.o \
				system.o direct_tty.o page.o memory.o clock.o sys_sportio.o \
				apic_asm.o fpu.o hpet.o tsc.o watchdog.o apic_flat.o x2apic_phys.o \
				setup_cpulocals.o tls.o

ifeq ($(CONFIG_SMP),y)
	KERNELOBJS += smp.o trampoline.o trampoline_32.o
endif

ifeq ($(CONFIG_KVM_GUEST), y)
	KERNELOBJS += kvm.o kvmclock.o
endif

ARCHKERNEL		= arch-kernel.o
LINKERSCRIPT	= lyos.lds

everything : realclean $(ARCHKERNEL) .linker_script

realclean :
	@rm -f $(KERNELOBJS)

$(ARCHKERNEL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/arch/x86/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS)

.linker_script : lyos.lds.S
	@$(CPP) $(ASFLAGS) -E -P -D__ASSEMBLY__ -o $(LINKERSCRIPT) $<

%.o : %.asm
	@echo -e '\tASM\tkernel/arch/x86/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/x86/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
