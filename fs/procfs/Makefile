#
# Makefile for procfs.

PROCFS		= procfs
PROCFSOBJS = main.o root.o buffer.o file.o cpuinfo.o memory.o tree.o pid.o
LIBS 		= -lmemfs -lfsdriver -lbdev -llyos

.PHONY: everything realclean

everything : realclean $(PROCFS)

realclean :
	@rm -f $(PROCFS)

$(PROCFS): $(PROCFSOBJS)
	@echo -e '\tLD\tfs/procfs/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) -Xlinker -Ttext -Xlinker 0x30000000

%.o: %.c
	@echo -e '\tCC\tfs/procfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
