##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ARFLAGS		= rcs

LOBJS		= arch/$(ARCH)/arch-lib.o \
			printl.o spin.o time.o data_copy.o procctl.o mmap.o list.o get_info.o \
			vmctl.o privctl.o umap.o env.o panic.o irqctl.o fork.o clear.o

LIB		= $(LIBOUTDIR)/liblyos.a

.PHONY: everything realclean 
	
everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

arch/$(ARCH)/arch-lib.o:
	@(cd arch/$(ARCH)/; make)
 
%.o: %.c
	@echo -e '\tCC\tlib/liblyos/$@'
	@$(CC) $(CFLAGS) -o $@ $<
