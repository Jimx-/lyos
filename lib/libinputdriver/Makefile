#
# Makefile for libinputdriver.
#

LIBINPUTDRVOBJS		= inputdriver.o
LIBINPUTDRV 		= $(LIBOUTDIR)/libinputdriver.a
ARFLAGS				= rcs

everything : realclean $(LIBINPUTDRV)

realclean :
	@rm -f $(LIBINPUTDRVOBJS)

$(LIBINPUTDRV): $(LIBINPUTDRVOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libinputdriver/$@'
	@$(CC) $(CFLAGS) -o $@ $<
