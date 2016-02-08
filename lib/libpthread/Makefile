#
# Makefile for libpthread.
#

LIBPTHREADOBJS	= pthread_attr.o pthread_create.o pthread_exit.o
LIBPTHREAD 		= $(LIBOUTDIR)/libpthread.a
ARFLAGS			= rcs

everything : realclean $(LIBPTHREAD)

realclean :
	@rm -f $(LIBPTHREADOBJS)

$(LIBPTHREAD): $(LIBPTHREADOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libpthread/$@'
	@$(CC) $(CFLAGS) -o $@ $<
