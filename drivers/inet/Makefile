#
# Makefile for inet driver.

INETOBJS 	= main.o
INET		= inet

LIBS	= -llyos

everything : realclean $(INET)

realclean :
	@rm -f $(INET) $(INETOBJS)

$(INET): $(INETOBJS)
	@echo -e '\tCC\tdrivers/inet/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tdrivers/inet/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
