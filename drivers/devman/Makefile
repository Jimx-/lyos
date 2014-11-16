#
# Makefile for device manager.

DEVMAN		= devman
DEVMANOBJS = main.o ddmap.o global.o
LIBS 		= -llyos

everything : realclean $(DEVMAN)

realclean :
	@rm -f $(DEVMAN) $(DEVMANOBJS)

$(DEVMAN): $(DEVMANOBJS)
	@echo -e '\tLD\tdrivers/devman/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tdrivers/devman/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


