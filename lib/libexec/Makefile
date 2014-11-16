#
# Makefile for libexec.
#

LIBEXECOBJS		= exec_elf.o exec_general.o
LIBEXEC 		= $(LIBOUTDIR)/libexec.a
ARFLAGS			= rcs

everything : realclean $(LIBEXEC)

realclean :
	@rm -f $(LIBEXECOBJS)

$(LIBEXEC): $(LIBEXECOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libexec/$@'
	@$(CC) $(CFLAGS) -o $@ $<
