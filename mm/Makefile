#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

MMOBJS 		= arch/$(ARCH)/arch-mm.o \
			main.o forkexit.o pagetable.o signal.o alloc.o region.o vmalloc.o brk.o \
			pagefault.o misc.o mmap.o global.o
MM		= mm.o

everything : realclean $(MM)

realclean :
	@rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	@echo -e '\tLD\tmm/$@'
	@$(LD) -r -o $(MM) $(MMOBJS)

arch/$(ARCH)/arch-mm.o:
	@(cd arch/$(ARCH)/; make)

%.o: %.c
	@echo -e '\tCC\tmm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

