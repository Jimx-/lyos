#
# Makefile for the Lyos filesystem.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

LIBS		= $(LIBDIR)/libexec/libexec.a
FSOBJS		= main.o super.o open.o mount.o global.o path.o inode.o protect.o \
				read_write.o stat.o link.o misc.o exec.o device.o \
				ext2/ext2fs.o initfs/initfs.o $(LIBS)

PROCFS		= procfs/procfs

FS			= fs.o

everything : realclean $(FS) $(PROCFS)

realclean :
	@rm -f $(FSOBJS) $(PROCFS)

$(FS): $(FSOBJS)
	@echo -e '\tLD\tfs/$@'
	@$(LD) -r -o $(FS) $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<

ext2/ext2fs.o:
	@(cd ext2; make)

initfs/initfs.o:
	@(cd initfs; make)

$(PROCFS):
	@(cd procfs; make)
	@(cp $(PROCFS) ../sysroot/sbin/)

$(LIBDIR)/libexec/libexec.a:
	@(cd $(LIBDIR)/libexec; make)
