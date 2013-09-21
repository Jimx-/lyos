#
# Makefile for the Lyos filesystem.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

FSOBJS		= main.o super.o open.o mount.o global.o path.o inode.o protect.o \
				read_write.o stat.o link.o \
				ext2/ext2fs.o

FS			= fs.o

everything : realclean $(FS)

realclean :
	@rm -f $(FSOBJS)

$(FS): $(FSOBJS)
	@echo -e '\tLD\tfs/$@'
	@$(LD) -r -o $(FS) $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<

ext2/ext2fs.o:
	@(cd ext2; make)

