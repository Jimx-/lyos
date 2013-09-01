#
# Makefile for libdriver.
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBDRVOBJS		= driver.o
LIBDRIVER		= libdriver.a
ARFLAGS			= rcs

everything : realclean $(LIBDRIVER)

realclean :
	@rm -f $(LIBDRVOBJS)

$(LIBDRIVER): $(LIBDRVOBJS)
	@echo -e '\tAR\tdrivers/libdriver/$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/libdriver/$@'
	@$(CC) $(CFLAGS) -o $@ $<
