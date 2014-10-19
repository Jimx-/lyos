#
# Makefile for pci driver.

PCICFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

PCI		= pci
PCIOBJS = main.o pci_table.o

LIBS	= $(LIBDIR)/liblyos/liblyos.a

everything : realclean $(PCI)

realclean :
	@rm -f $(PCI) $(PCIOBJS)

$(PCI): $(PCIOBJS) $(LIBS)
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(PCICFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(PCICFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)
