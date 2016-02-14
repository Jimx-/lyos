
FSOBJS 	= main.o buffer.o super.o path.o global.o utils.o stat.o read_write.o
LIBS	= -lbdev -llyos 

INITFS = initfs

everything : realclean $(INITFS)

all : realclean $(INITFS)

realclean :
	@rm -f $(INITFS) $(FSOBJS)

$(INITFS): $(FSOBJS)
	@echo -e '\tLD\tfs/initfs/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) 

%.o: %.c
	@echo -e '\tCC\tfs/initfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
