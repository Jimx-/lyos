
MMOBJS		= page.o _page.o

ARCHMM		= arch-mm.o

everything : realclean $(ARCHMM)

realclean :
	@rm -f $(MMOBJS)

$(ARCHMM): $(MMOBJS)
	@echo -e '\tLD\tmm/arch/x86/$@'
	@$(LD) -r -o $(ARCHMM) $(MMOBJS)

_page.o : page.asm
	@echo -e '\tASM\tmm/arch/x86/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

page.o : page.c
	@echo -e '\tCC\tmm/arch/x86/$@'
	@$(CC) $(CFLAGS) -o $@ $<
