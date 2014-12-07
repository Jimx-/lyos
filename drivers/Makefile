#
# Makefile for Lyos driver.

LIBS 		= $(LIBOUTDIR)/libdriver.a
INET 		= inet/inet
PCI 		= pci/pci

.PHONY: everything realclean block char
	
everything : realclean block char $(INET) $(PCI)

realclean :
	@rm -f $(INET) $(PCI)

block:
	@(cd block; make)

char:
	@(cd char; make)

$(PCI):
	@(cd pci; make)
	@(cp -f $(PCI) ../sysroot/sbin/)

$(INET):
	@(cd inet; make)
	@(cp -f $(INET) ../sysroot/usr/sbin/)
