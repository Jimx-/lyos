#
# Makefile for Lyos driver.

LIBS 		= $(LIBOUTDIR)/libdriver.a
DRVOBJS		= char/char.o firmware/firmware.o 
INET 		= inet/inet
PCI 		= pci/pci
DRIVER		= drivers.o

.PHONY: everything realclean block 
	
everything : realclean block $(DRIVER) $(INET) $(PCI)

realclean :
	@rm -f $(DRIVER) $(DRVOBJS) $(INET) $(PCI)

block:
	@(cd block; make)

$(DRIVER): $(DRVOBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $(DRIVER) $(DRVOBJS) $(LIBS)

char/char.o: $(LIBS)
	@(cd char; make)

net/inet.o: $(LIBS)
	@(cd net; make)

$(PCI):
	@(cd pci; make)
	@(cp -f $(PCI) ../sysroot/sbin/)

$(INET):
	@(cd inet; make)
	@(cp -f $(INET) ../sysroot/usr/sbin/)

firmware/firmware.o: $(LIBS)
	@(cd firmware; make)
