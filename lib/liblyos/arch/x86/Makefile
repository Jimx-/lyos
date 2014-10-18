LOBJS		= ioport.o

ARCHLIB		= arch-lib.o

everything : realclean $(ARCHLIB)

realclean :
	@rm -f $(LOBJS)

$(ARCHLIB): $(LOBJS)
	@echo -e '\tLD\tlib/arch/x86/$@'
	@$(LD) -r -o $(ARCHLIB) $(LOBJS)

%.o : %.asm
	@echo -e '\tASM\tlib/arch/x86/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

