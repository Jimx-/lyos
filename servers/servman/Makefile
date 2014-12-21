#
# Makefile for the Lyos memory manager.

SERVMANOBJS = main.o service.o exec.o global.o
LIBS 		= -lsysfs -lexec -llyos
SERVMAN		= servman

everything : realclean $(SERVMAN)

realclean :
	@rm -f $(SERVMAN) $(SERVMANOBJS)

$(SERVMAN): $(SERVMANOBJS)
	@echo -e '\tLD\tservers/servman/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/servman/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
