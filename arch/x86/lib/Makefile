ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf

LOBJS		= syscall.o ioport.o

X86LIB		= x86-lib.o

everything : realclean $(X86LIB)

realclean :
	@rm -f $(LOBJS)

$(X86LIB): $(LOBJS)
	@echo -e '\tLD\tarch/x86/lib/$@'
	@$(LD) -r -o $(X86LIB) $(LOBJS)

%.o : %.asm
	@echo -e '\tASM\tarch/x86/kernel/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

