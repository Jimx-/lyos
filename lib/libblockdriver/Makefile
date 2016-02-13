#
# Makefile for libdriver.
#

LIBBDRVOBJS		= driver.o
LIBBDRIVER		= $(LIBOUTDIR)/libblockdriver.a
ARFLAGS			= rcs

everything : realclean $(LIBBDRIVER)

realclean :
	@rm -f $(LIBBDRVOBJS)

$(LIBBDRIVER): $(LIBBDRVOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libblockdriver/$@'
	@$(CC) $(CFLAGS) -o $@ $<
