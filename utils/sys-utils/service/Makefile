
SERVICE			= $(DESTDIR)/bin/service
SERVICEOBJS 	= main.o

everything : realclean $(SERVICE)

realclean :
	@rm -f $(SERVICE) $(SERVICEOBJS)

$(SERVICE): $(SERVICEOBJS)
	@$(CC) $(SERVERCFLAGS) -o $@ $^

%.o: %.c
	@$(CC) $(SERVERCFLAGS) -c $<
