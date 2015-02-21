
FSOBJS 	= main.o super.o global.o inode.o protect.o path.o read_write.o buffer.o \
			alloc.o stat.o open.o link.o
LIBS	= -llyos -ldriver

EXT2FS = ext2fs

everything : realclean $(EXT2FS)

all : realclean $(EXT2FS)

realclean :
	@rm -f $(EXT2FS) $(FSOBJS)

$(EXT2FS): $(FSOBJS)
	@echo -e '\tLD\tfs/ext2/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)
	
%.o: %.c
	@echo -e '\tCC\tfs/ext2/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
