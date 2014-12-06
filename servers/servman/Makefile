#
# Makefile for the Lyos memory manager.

SERVMANOBJS = main.o service.o exec.o module.o global.o
LIBS 		= $(LIBOUTDIR)/libsysfs.a
SERVMAN		= servman.o 

everything : realclean $(SERVMAN)

realclean :
	@rm -f $(SERVMANOBJS) $(SERVMAN)

$(SERVMAN): $(SERVMANOBJS)
	@echo -e '\tLD\tservers/servman/$@'
	@$(LD) -r -o $(SERVMAN) $(SERVMANOBJS) $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/servman/$@'
	@$(CC) $(CFLAGS) -o $@ $<
