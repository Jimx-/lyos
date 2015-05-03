#
# Makefile for device manager.

DEVMAN		= devman
DEVMANOBJS = main.o ddmap.o global.o
LIBS 		= -llyos -lsysfs

everything : realclean $(DEVMAN)

realclean :
	@rm -f $(DEVMAN) $(DEVMANOBJS)

$(DEVMAN): $(DEVMANOBJS)
	@echo -e '\tLD\tservers/devman/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) 

%.o: %.c
	@echo -e '\tCC\tservers/devman/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


