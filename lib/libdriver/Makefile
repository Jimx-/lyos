#
# Makefile for libdriver.
#

LIBDRVOBJS		= driver.o bdev.o
LIBDRIVER		= $(LIBOUTDIR)/libdriver.a
ARFLAGS			= rcs

everything : realclean $(LIBDRIVER)

realclean :
	@rm -f $(LIBDRVOBJS)

$(LIBDRIVER): $(LIBDRVOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libdriver/$@'
	@$(CC) $(CFLAGS) -o $@ $<
