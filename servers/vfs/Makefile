#
# Makefile for the Lyos filesystem.

LIBS		= $(LIBOUTDIR)/libexec.a
FSOBJS		= main.o super.o open.o mount.o global.o path.o inode.o protect.o \
				read_write.o stat.o link.o misc.o exec.o device.o

VFS			= vfs.o

everything : realclean $(VFS)

realclean :
	@rm -f $(VFS) $(FSOBJS)

$(VFS): $(FSOBJS)
	@echo -e '\tLD\tservers/vfs/$@'
	@$(LD) -r -o $(VFS) $(FSOBJS) $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/vfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<
