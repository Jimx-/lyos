#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

MMOBJS 		= main.o fork.o pagetable.o alloc.o region.o vmalloc.o brk.o \
			pagefault.o misc.o mmap.o global.o utils.o
LIBS		= $(LIBOUTDIR)/libexec.a
MM			= mm.o

everything : realclean $(MM)

realclean :
	@rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	@echo -e '\tLD\tservers/mm/$@'
	@$(LD) -r -o $(MM) $(MMOBJS) $(LIBS)

arch/$(ARCH)/arch-mm.o:
	@(cd arch/$(ARCH)/; make)

%.o: %.c
	@echo -e '\tCC\tservers/mm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

