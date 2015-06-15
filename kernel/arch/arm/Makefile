
KERNELOBJS	= kernel.o start.o system.o memory.o fpu.o direct_tty.o clock.o \
				page.o kliba.o

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
