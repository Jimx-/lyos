#
# Makefile for procfs.

PROCFSCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

PROCFS		= procfs
PROCFSOBJS = main.c
LIBS 		= $(LIBDIR)/liblyos/liblyos.a

everything : realclean $(PROCFS)

realclean :
	@rm -f $(PROCFS)

$(PROCFS): $(PROCFSOBJS) $(LIBS)
	@echo -e '\tLD\tfs/procfs/$@'
	@$(CC) $(PROCFSCFLAGS) -o $@ $^

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)
