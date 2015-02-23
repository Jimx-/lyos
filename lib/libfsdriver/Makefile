#
# Makefile for libfsdriver.
#

LIBFSDOBJS		= request.o fsdriver.o utils.o
LIBFSD 		= $(LIBOUTDIR)/libfsdriver.a
ARFLAGS			= rcs

everything : realclean $(LIBFSD)

realclean :
	@rm -f $(LIBFSDOBJS)

$(LIBFSD): $(LIBFSDOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libfsdrvier/$@'
	@$(CC) $(CFLAGS) -o $@ $<
