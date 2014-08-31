
KERNELOBJS	= kernel.o

ARCHKERNEL		= arch-kernel.o

everything : realclean $(ARCHKERNEL)

realclean :
	@rm -f $(KERNELOBJS)

$(ARCHKERNEL): $(KERNELOBJS)
	@echo -e '\tLD\tkernel/arch/arm/$@'
	@$(LD) -r -o $(ARCHKERNEL) $(KERNELOBJS)

%.o : %.S
	@echo -e '\tAS\tkernel/arch/arm/$@'
	@$(AS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tkernel/arch/arm/$@'
	@$(CC) $(CFLAGS) -o $@ $<
