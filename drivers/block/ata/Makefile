ATACFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/  -Wall

ATAOBJS 	= main.o
LIBS	= $(LIBDIR)/liblyos/liblyos.a $(LIBDIR)/libdriver/libdriver.a

ATA = ata

everything : realclean $(ATA)

all : realclean $(ATA)

realclean :
	@rm -f $(ATA) $(ATAOBJS)

$(ATA): $(ATAOBJS) $(LIBS)
	@echo -e '\tLD\tdrivers/block/ata/$@'
	@$(CC) $(ATACFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/block/ata/$@'
	@$(CC) $(ATACFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)

