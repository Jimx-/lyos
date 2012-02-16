ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	=  -I $(INCDIR)/ -I $(ARCHINC)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINC)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

KERNELOBJS	= kernel.o kliba.o reboot.o i8259.o start.o protect.o

X86KRNL		= x86-krnl.o

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

i8259.o: i8259.c
	$(CC) $(CFLAGS) -o $@ $<

protect.o: protect.c
	$(CC) $(CFLAGS) -o $@ $<

start.o: start.c
	$(CC) $(CFLAGS) -o $@ $<

