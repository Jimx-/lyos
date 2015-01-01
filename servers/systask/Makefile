#
# Makefile for process manager.

SYSTASK			= systask
SYSTASKOBJS 	= main.o
LIBS 			= -llyos

everything : realclean $(SYSTASK)

realclean :
	@rm -f $(SYSTASK) $(SYSTASKOBJS)

$(SYSTASK): $(SYSTASKOBJS)
	@echo -e '\tLD\tservers/systask/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/systask/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


