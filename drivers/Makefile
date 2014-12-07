#
# Makefile for Lyos driver.

LIBS 		= $(LIBOUTDIR)/libdriver.a
DRVOBJS		= firmware/firmware.o 
INET 		= inet/inet
PCI 		= pci/pci
DRIVER		= drivers.o

.PHONY: everything realclean block char
	
everything : realclean block char $(DRIVER) $(INET) $(PCI)

realclean :
	@rm -f $(DRIVER) $(DRVOBJS) $(INET) $(PCI)

block:
	@(cd block; make)

char:
	@(cd char; make)

$(DRIVER): $(DRVOBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $(DRIVER) $(DRVOBJS) $(LIBS)

$(PCI):
	@(cd pci; make)
	@(cp -f $(PCI) ../sysroot/sbin/)

$(INET):
	@(cd inet; make)
	@(cp -f $(INET) ../sysroot/usr/sbin/)

firmware/firmware.o: $(LIBS)
	@(cd firmware; make)
