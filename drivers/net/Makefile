#
# Makefile for inet driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

INET		= inet.o

everything : realclean $(INET)

realclean :
	@rm -f $(INET)

$(INET): inet.c
	@echo -e '\tCC\tdrivers/net/$@'
	@$(CC) $(CFLAGS) -o $@ $<


