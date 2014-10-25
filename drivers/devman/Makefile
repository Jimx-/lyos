#
# Makefile for device manager.

DEVMANCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -Wall

DEVMAN		= devman
DEVMANOBJS = main.o ddmap.o global.o
LIBS 		= $(LIBDIR)/liblyos/liblyos.a

everything : realclean $(DEVMAN)

realclean :
	@rm -f $(DEVMAN) $(DEVMANOBJS)

$(DEVMAN): $(DEVMANOBJS) $(LIBS)
	@echo -e '\tLD\tdrivers/devman/$@'
	@$(CC) $(DEVMANCFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/devman/$@'
	@$(CC) $(DEVMANCFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)

