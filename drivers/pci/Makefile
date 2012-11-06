#
# Makefile for pci driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

PCI		= pci.o

everything : reclean $(PCI)

reclean :
	@rm -f $(PCI)

$(PCI): pci.c
	@echo -e '\tCC\tdrivers/pci/$@'
	@$(CC) $(CFLAGS) -o $@ $<


