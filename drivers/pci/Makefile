#
# Makefile for pci driver.

PCI		= pci
PCIOBJS = main.o pci_table.o

LIBS	= -llyos

everything : realclean $(PCI)

realclean :
	@rm -f $(PCI) $(PCIOBJS)

$(PCI): $(PCIOBJS)
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
