LOBJS		= spinlock.o
ARCHLIB		= arch-lib.o

everything : realclean $(ARCHLIB)

realclean :
	@rm -f $(LOBJS)

$(ARCHLIB): $(LOBJS)
	@echo -e '\tLD\tlib/liblyos/arch/arm/$@'
	@$(LD) -r -o $(ARCHLIB) $(LOBJS)

%.o : %.S
	@echo -e '\tAS\tlib/liblyos/arch/arm/$@'
	@$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tlib/liblyos/arch/arm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

