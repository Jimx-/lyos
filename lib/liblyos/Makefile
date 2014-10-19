##############################
# Makefile for Lyos library. #
##############################

AR		= ar
ARFLAGS		= rcs

LOBJS		= arch/$(ARCH)/arch-lib.o \
			printl.o spin.o time.o data_copy.o procctl.o mmap.o list.o get_info.o vmctl.o privctl.o

LIB		= liblyos.a

everything : realclean $(LIB)

realclean :
	@rm -f $(LOBJS)

$(LIB): $(LOBJS)
	@echo -e '\tAR\tlib/liblyos/$@'
	@$(AR) $(ARFLAGS) $@ $^

arch/$(ARCH)/arch-lib.o:
	@(cd arch/$(ARCH)/; make)
 
%.o: %.c
	@echo -e '\tCC\tlib/liblyos/$@'
	@$(CC) $(CFLAGS) -o $@ $<
