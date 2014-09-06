INITFSCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

FSOBJS 	= main.o buffer.o super.o path.o global.o utils.o stat.o read_write.o
LIBS	= $(LIBDIR)/liblyos/liblyos.a $(LIBDIR)/libdriver/libdriver.a

INITFS = initfs

everything : realclean $(INITFS)

all : realclean $(INITFS)

realclean :
	@rm -f $(INITFS) $(FSOBJS)

$(INITFS): $(FSOBJS) $(LIBS)
	@echo -e '\tLD\tfs/initfs/$@'
	@$(CC) $(INITFSCFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tfs/initfs/$@'
	@$(CC) $(INITFSCFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)

