ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

LOBJS	= syscall.o string.o

X86LIB		= x86-lib.o

everything : reclean $(X86LIB)

reclean :
	@rm -f $(LOBJS)

$(X86LIB): $(LOBJS)
	@echo -e '\tLD\tarch/x86/lib/$@'
	@$(LD) -r -o $(X86LIB) $(LOBJS)

%.o : %.asm
	@echo -e '\tASM\tarch/x86/kernel/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

