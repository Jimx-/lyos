#
# Makefile for the Lyos filesystem.

VFS 		= vfs
FSOBJS		= main.o super.o open.o mount.o global.o path.o inode.o protect.o \
				read_write.o stat.o link.o misc.o exec.o device.o thread.o file.o
LIBS 		= -lexec -llyos

everything : realclean $(VFS)

realclean :
	@rm -f $(VFS) $(FSOBJS)

$(VFS): $(FSOBJS)
	@echo -e '\tLD\tservers/vfs/$@'
	@$(CC) $(SERVERCFLAGS) -Wl,-Ttext,0x80000000 -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/vfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
