
MMOBJS		= 

ARCHMM		= arch-mm.o

everything : realclean $(ARCHMM)

realclean :
	@rm -f $(MMOBJS)

$(ARCHMM): $(MMOBJS)
	@echo -e '\tLD\tmm/arch/x86/$@'
	@$(LD) -r -o $(ARCHMM) $(MMOBJS)

page.o : page.c
	@echo -e '\tCC\tmm/arch/x86/$@'
	@$(CC) $(CFLAGS) -o $@ $<
