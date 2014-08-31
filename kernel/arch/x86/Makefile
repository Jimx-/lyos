
KERNELOBJS	= kernel.o kliba.o i8259.o start.o protect.o

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
	@$(CC) $(CFLAGS) -o $@ $<
