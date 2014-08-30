#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

SERVMANOBJS 		= main.o service.o exec.o module.o
SERVMAN		= servman.o

everything : realclean $(SERVMAN)

realclean :
	@rm -f $(SERVMANOBJS) $(SERVMAN)

$(SERVMAN): $(SERVMANOBJS)
	@echo -e '\tLD\tservman/$@'
	@$(LD) -r -o $(SERVMAN) $(SERVMANOBJS)

%.o: %.c
	@echo -e '\tCC\tservman/$@'
	@$(CC) $(CFLAGS) -o $@ $<

