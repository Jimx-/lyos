
SERVICE			= $(DESTDIR)/bin/service
SERVICEOBJS 	= main.o parse.o cJSON/libcjson.a

everything : realclean $(SERVICE)

realclean :
	@rm -f $(SERVICE) $(SERVICEOBJS)

$(SERVICE): $(SERVICEOBJS)
	@$(CC) $(SERVERCFLAGS) -o $@ $^ -lm

cJSON/libcjson.a:
	@(cd cJSON; make libcjson.a)

%.o: %.c
	@$(CC) $(SERVERCFLAGS) -c $< -I./cJSON
