##############################
# Makefile for Lyos kernel.  #
##############################

LIBOBJS	= user_stubs.o string.o vsprintf.o

KLIBC		= klibc.o

everything : realclean $(KLIBC)

realclean :
	@rm -f $(LIBOBJS)

$(KLIBC): $(LIBOBJS)
	@echo -e '\tLD\tkernel/lib/$@'
	@$(LD) -r -o $(KLIBC) $(LIBOBJS)

%.o: %.c
	@echo -e '\tCC\tkernel/lib/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
