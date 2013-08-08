#
# Makefile for pci driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

PCI		= pci.o

everything : realclean $(PCI)

realclean :
	@rm -f $(PCI)

$(PCI): pci.c
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(CFLAGS) -o $@ $<


