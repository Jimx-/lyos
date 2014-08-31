#
# Makefile for device manager.

DEVMANCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

DEVMAN		= devman
DEVMANOBJS = main.c ddmap.c global.c
LIBS 		= $(LIBDIR)/liblyos/liblyos.a

everything : realclean $(DEVMAN)

realclean :
	@rm -f $(DEVMAN)

$(DEVMAN): $(DEVMANOBJS) $(LIBS)
	@echo -e '\tLD\tdrivers/devman/$@'
	@$(CC) $(DEVMANCFLAGS) -o $@ $^

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)
