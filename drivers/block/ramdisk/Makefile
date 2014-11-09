RAMDISKCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/  -Wall

RDOBJS 	= main.o
LIBS	= $(LIBDIR)/liblyos/liblyos.a $(LIBDIR)/libdriver/libdriver.a

RAMDISK = ramdisk

everything : realclean $(RAMDISK)

all : realclean $(RAMDISK)

realclean :
	@rm -f $(RAMDISK) $(RDOBJS)

$(RAMDISK): $(RDOBJS) $(LIBS)
	@echo -e '\tLD\tdrivers/block/ramdisk/$@'
	@$(CC) $(RAMDISKCFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/block/ramdisk/$@'
	@$(CC) $(RAMDISKCFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)

