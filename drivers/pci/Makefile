#
# Makefile for pci driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

PCI		= pci.o
PCIOBJS = main.o pci_table.o

everything : realclean $(PCI)

realclean :
	@rm -f $(PCI) $(PCIOBJS)

$(PCI): $(PCIOBJS)
	@echo -e '\tLD\tdrivers/pci/$@'
	@$(LD) -r -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(CFLAGS) -o $@ $<
