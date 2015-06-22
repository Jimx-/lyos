#
# Makefile for process manager.

PM			= pm
PMOBJS 		= main.o global.o utils.o forkexit.o signal.o misc.o trace.o exec.o
LIBS 		= -llyos

everything : realclean $(PM)

realclean :
	@rm -f $(PM) $(PMOBJS)

$(PM): $(PMOBJS)
	@echo -e '\tLD\tservers/pm/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)  -Xlinker -Ttext -Xlinker 0x8000

%.o: %.c
	@echo -e '\tCC\tservers/pm/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


