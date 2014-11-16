#
# Makefile for procfs.

PROCFS		= procfs
PROCFSOBJS = main.o
LIBS 		= -llyos

.PHONY: everything realclean

everything : realclean $(PROCFS)

realclean :
	@rm -f $(PROCFS)

$(PROCFS): $(PROCFSOBJS)
	@echo -e '\tLD\tfs/procfs/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tfs/procfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
