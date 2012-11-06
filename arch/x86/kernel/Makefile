ASMKFLAGS	=  -I $(INCDIR)/ -I $(ARCHINC)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

KERNELOBJS	= kernel.o kliba.o reboot.o i8259.o start.o protect.o

X86KRNL		= x86-krnl.o

everything : reclean $(X86KRNL)

reclean :
	@rm -f $(KERNELOBJS)

$(X86KRNL): $(KERNELOBJS)
	@echo -e '\tLD\tarch/x86/kernel/$@'
	@$(LD) -r -o $(X86KRNL) $(KERNELOBJS)

%.o : %.asm
	@echo -e '\tASM\tarch/x86/kernel/$@'
	@$(ASM) $(ASMKFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tarch/x86/kernel/$@'
	@$(CC) $(CFLAGS) -o $@ $<


