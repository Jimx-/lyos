#
# Makefile for the Lyos memory manager.


MM			= mm
MMOBJS 		= main.o fork.o pagetable.o alloc.o region.o vmalloc.o brk.o \
			pagefault.o misc.o mmap.o global.o utils.o slab.o
LIBS 		= -lexec -llyos 

everything : realclean $(MM)

realclean :
	@rm -f $(MM) $(MMOBJS)

$(MM): $(MMOBJS)
	@echo -e '\tLD\tservers/mm/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) 

arch/$(ARCH)/arch-mm.o:
	@(cd arch/$(ARCH)/; make)

%.o: %.c
	@echo -e '\tCC\tservers/mm/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
