SYSFSCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

FSOBJS 	= main.o
LIBS	= $(LIBDIR)/liblyos/liblyos.a $(LIBDIR)/libdriver/libdriver.a

SYSFS = sysfs

everything : realclean $(SYSFS)

all : realclean $(SYSFS)

realclean :
	@rm -f $(SYSFS) $(FSOBJS)

$(SYSFS): $(FSOBJS) $(LIBS)
	@echo -e '\tLD\tfs/sysfs/$@'
	@$(CC) $(SYSFSCFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tfs/sysfs/$@'
	@$(CC) $(SYSFSCFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)

$(LIBDIR)/libdriver/libdriver.a:
	@(cd $(LIBDIR)/libdriver; make)

