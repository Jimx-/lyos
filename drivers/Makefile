#
# Makefile for Lyos driver.

LIBS 		= $(LIBOUTDIR)/libdriver.a
INET 		= inet/inet
PCI 		= pci/pci

.PHONY: everything realclean block char hid
	
everything : realclean block char hid $(INET) $(PCI)

realclean :
	@rm -f $(INET) $(PCI)

block:
	@(cd block; make)

char:
	@(cd char; make)

hid:
	@(cd hid; make)

$(PCI):
	@(cd pci; make)
	@(cp -f $(PCI) $(DESTDIR)/sbin)

$(INET):
	@(cd inet; make)
	@(cp -f $(INET) $(DESTDIR)/usr/sbin/)
