#
# Makefile for libexec.
#

LIBEXECOBJS		= exec_elf.o
LIBEXEC 		= libexec.a
ARFLAGS			= rcs

everything : realclean $(LIBEXEC)

realclean :
	@rm -f $(LIBEXECOBJS)

$(LIBEXEC): $(LIBEXECOBJS)
	@echo -e '\tAR\tlib/libexec/$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libexec/$@'
	@$(CC) $(CFLAGS) -o $@ $<
