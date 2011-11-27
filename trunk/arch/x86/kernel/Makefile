ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

KERNELOBJS	= kernel.o kliba.o reboot.o

X86KRNL		= x86_krnl.o

everything : reclean $(X86KRNL)

reclean :
	rm -f $(KERNELOBJS)

$(X86KRNL): $(KERNELOBJS)
	$(LD) -r -o $(X86KRNL) $(KERNELOBJS)

kernel.o : kernel.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

reboot.o : reboot.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

kliba.o : kliba.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<
