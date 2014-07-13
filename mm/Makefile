#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

MMOBJS 		= ../arch/$(ARCH)/mm/$(ARCH)-mm.o \
			main.o forkexit.o exec.o memory.o signal.o alloc.o region.o vmalloc.o brk.o \
			service.o fault.o
MM		= mm.o

everything : realclean $(MM)

realclean :
	@rm -f $(MMOBJS)

$(MM): $(MMOBJS)
	@echo -e '\tLD\tmm/$@'
	@$(LD) -r -o $(MM) $(MMOBJS)

../arch/$(ARCH)/mm/$(ARCH)-mm.o:
	@(cd ../arch/$(ARCH)/mm; make)

%.o: %.c
	@echo -e '\tCC\tmm/$@'
	@$(CC) $(CFLAGS) -o $@ $<

