#
# Makefile for the Lyos memory manager.


SERVICE			= service
SERVICEOBJS 	= main.o

everything : realclean $(SERVICE)

realclean :
	@rm -f $(SERVICE) $(SERVICEOBJS)

$(SERVICE): $(SERVICEOBJS)
	@$(CC) $(SERVERCFLAGS) -o $@ $^ -Xlinker -Map -Xlinker a.map

%.o: %.c
	@$(CC) $(SERVERCFLAGS) -c $<
