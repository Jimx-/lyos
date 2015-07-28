#
# Makefile for libdevman.
#

LIBDMOBJS		= bus_type.o device.o
LIBDEVMAN		= $(LIBOUTDIR)/libdevman.a
ARFLAGS			= rcs

everything : realclean $(LIBDEVMAN)

realclean :
	@rm -f $(LIBDMOBJS)

$(LIBDEVMAN): $(LIBDMOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libdevman/$@'
	@$(CC) $(CFLAGS) -o $@ $<
