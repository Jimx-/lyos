ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

MMOBJS		= page.o

X86MM		= x86_mm.o

everything : reclean $(X86MM)

reclean :
	rm -f $(MMOBJS)

$(X86MM): $(MMOBJS)
	$(LD) -r -o $(X86MM) $(MMOBJS)

page.o : page.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<
