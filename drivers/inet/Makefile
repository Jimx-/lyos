#
# Makefile for inet driver.
INETCFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/

INETOBJS 	= main.o
INET		= inet

LIBS	= $(LIBDIR)/liblyos/liblyos.a

everything : realclean $(INET)

realclean :
	@rm -f $(INET) $(INETOBJS)

$(INET): $(INETOBJS) $(LIBS)
	@echo -e '\tCC\tdrivers/inet/$@'
	@$(CC) $(INETCFLAGS) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tdrivers/inet/$@'
	@$(CC) $(INETCFLAGS) -c $<

$(LIBDIR)/liblyos/liblyos.a:
	@(cd $(LIBDIR)/liblyos; make)
