#
# Makefile for libchardriver.
#

LIBCDRVOBJS		= driver.o
LIBCDRIVER		= $(LIBOUTDIR)/libchardriver.a
ARFLAGS			= rcs

everything : realclean $(LIBCDRIVER)

realclean :
	@rm -f $(LIBCDRVOBJS)

$(LIBCDRIVER): $(LIBCDRVOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libchardriver/$@'
	@$(CC) $(CFLAGS) -o $@ $<
