LOBJS		= cpufeature.o cpuid.o portio.o spinlock.o

ARCHLIB		= arch-lib.o

everything : realclean $(ARCHLIB)

realclean :
	@rm -f $(LOBJS)

$(ARCHLIB): $(LOBJS)
	@echo -e '\tLD\tlib/liblyos/arch/x86/$@'
	@$(LD) -r -o $(ARCHLIB) $(LOBJS)

%.o : %.asm
	@echo -e '\tASM\tlib/liblyos/arch/x86/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tlib/liblyos/arch/x86/$@'
	@$(CC) $(CFLAGS) -o $@ $<

