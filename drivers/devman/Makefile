#
# Makefile for device manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

DEVMAN		= devman.o
DEVMANOBJS = main.o

everything : realclean $(DEVMAN)

realclean :
	@rm -f $(DEVMAN) $(DEVMANOBJS)

$(DEVMAN): $(DEVMANOBJS)
	@echo -e '\tLD\tdrivers/devman/$@'
	@$(LD) -r -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/devman/$@'
	@$(CC) $(CFLAGS) -o $@ $<
