
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

MMOBJS		= page.o

X86MM		= x86-mm.o

everything : realclean $(X86MM)

realclean :
	@rm -f $(MMOBJS)

$(X86MM): $(MMOBJS)
	@echo -e '\tLD\tarch/x86/mm/$@'
	@$(LD) -r -o $(X86MM) $(MMOBJS)

%.o : %.asm
	@echo -e '\tASM\tarch/x86/mm/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<
