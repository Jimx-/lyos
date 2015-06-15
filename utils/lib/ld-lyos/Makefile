
LDLYOS			= $(DESTDIR)/lib/ld-lyos.so
LDLYOSOBJS 		= main.o

everything : realclean $(LDLYOS)

realclean :
	@rm -f $(LDLYOS) $(LDLYOSOBJS)

$(LDLYOS): $(LDLYOSOBJS)
	@$(CC) -o $@ $^

%.o: %.c
	@$(CC) -c $<
