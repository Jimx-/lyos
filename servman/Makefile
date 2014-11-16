#
# Makefile for the Lyos memory manager.

SERVMANOBJS 		= main.o service.o exec.o module.o
LIBS 		= $(LIBOUTDIR)/libsysfs.a
SERVMAN		= servman.o

everything : realclean $(SERVMAN)

realclean :
	@rm -f $(SERVMANOBJS) $(SERVMAN)

$(SERVMAN): $(SERVMANOBJS)
	@echo -e '\tLD\tservman/$@'
	@$(LD) -r -o $(SERVMAN) $(SERVMANOBJS) $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservman/$@'
	@$(CC) $(CFLAGS) -o $@ $<
