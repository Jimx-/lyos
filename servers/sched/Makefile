#
# Makefile for the Lyos userspace scheduler.

SCHEDOBJS 	= main.o
LIBS 		= -llyos
SCHED		= sched

everything : realclean $(SCHED)

realclean :
	@rm -f $(SCHED) $(SCHEDOBJS)

$(SCHED): $(SCHEDOBJS)
	@echo -e '\tLD\tservers/sched/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/sched/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
