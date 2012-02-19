#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

MMOBJS 		= ../arch/$(ARCH)/mm/$(ARCH)-mm.o \
			main.o forkexit.o exec.o memory.o signal.o alloc.o
MM		= mm.o

everything : reclean $(MM)

reclean :
	@rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	@echo -e '\tLD\tmm/$@'
	@$(LD) -r -o $(MM) $(MMOBJS)

../arch/$(ARCH)/mm/$(ARCH)-mm.o:
	@(cd ../arch/$(ARCH)/mm; make)

%.o: %.c
	@echo -e '\tCC\tmm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

