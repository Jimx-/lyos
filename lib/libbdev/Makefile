#
# Makefile for libbdev.
#

LIBBDEVOBJ		= driver.o bdev.o
LIBBDEV			= $(LIBOUTDIR)/libbdev.a
ARFLAGS			= rcs

everything : realclean $(LIBBDEV)

realclean :
	@rm -f $(LIBBDEVOBJ)

$(LIBBDEV): $(LIBBDEVOBJ)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libbdev/$@'
	@$(CC) $(CFLAGS) -o $@ $<
