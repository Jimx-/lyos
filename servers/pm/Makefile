#
# Makefile for process manager.

PM			= pm
PMOBJS 		= main.o global.o utils.o forkexit.o signal.o misc.o
LIBS 		= -llyos

everything : realclean $(PM)

realclean :
	@rm -f $(PM) $(PMOBJS)

$(PM): $(PMOBJS)
	@echo -e '\tLD\tservers/pm/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/pm/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


