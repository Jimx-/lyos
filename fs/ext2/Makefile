
FSOBJS 	= main.o super.o global.o inode.o protect.o path.o read_write.o buffer.o \
			alloc.o stat.o open.o link.o

EXT2FS = ext2fs.o

everything : realclean $(EXT2FS)

all : realclean $(EXT2FS)

realclean :
	@rm -f $(FSOBJS)

$(EXT2FS): $(FSOBJS)
	@echo -e '\tLD\tfs/ext2/$@'
	@$(LD) -r -o ext2fs.o $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/ext2/$@'
	@$(CC) $(CFLAGS) -o $@ $<
