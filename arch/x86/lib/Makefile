ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

LOBJS	= syscall.o string.o

X86LIB		= x86_lib.o

everything : reclean $(X86LIB)

reclean :
	rm -f $(LOBJS)

$(X86LIB): $(LOBJS)
	$(LD) -r -o $(X86LIB) $(LOBJS)

syscall.o : syscall.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

string.o : string.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<
