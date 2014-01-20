
FSOBJS 	= main.o buffer.o super.o path.o global.o utils.o stat.o read_write.o

INITFS = initfs.o

everything : realclean $(INITFS)

all : realclean $(INITFS)

realclean :
	@rm -f $(FSOBJS)

$(INITFS): $(FSOBJS)
	@echo -e '\tLD\tfs/initfs/$@'
	@$(LD) -r -o initfs.o $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/initfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<
