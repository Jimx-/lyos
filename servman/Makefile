#
# Makefile for the Lyos memory manager.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

SERVMANOBJS 		= main.o service.o exec.o module.o
LIBS 		= $(LIBDIR)/libsysfs/libsysfs.a
SERVMAN		= servman.o

everything : realclean $(SERVMAN)

realclean :
	@rm -f $(SERVMANOBJS) $(SERVMAN)

$(SERVMAN): $(SERVMANOBJS) $(LIBS)
	@echo -e '\tLD\tservman/$@'
	@$(LD) -r -o $(SERVMAN) $(SERVMANOBJS) $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservman/$@'
	@$(CC) $(CFLAGS) -o $@ $<

$(LIBDIR)/libsysfs/libsysfs.a:
	@(cd $(LIBDIR)/libsysfs; make)
