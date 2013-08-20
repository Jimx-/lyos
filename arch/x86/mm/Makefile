
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf

MMOBJS		= page.o _page.o

X86MM		= x86-mm.o

everything : realclean $(X86MM)

realclean :
	@rm -f $(MMOBJS)

$(X86MM): $(MMOBJS)
	@echo -e '\tLD\tarch/x86/mm/$@'
	@$(LD) -r -o $(X86MM) $(MMOBJS)

_page.o : page.asm
	@echo -e '\tASM\tarch/x86/mm/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

page.o : page.c
	@echo -e '\tCC\tarch/x86/mm/$@'
	@$(CC) $(CFLAGS) -o $@ $<
