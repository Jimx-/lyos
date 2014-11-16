#
# Makefile for the Lyos filesystem.

LIBS		= $(LIBOUTDIR)/libexec.a
FSOBJS		= main.o super.o open.o mount.o global.o path.o inode.o protect.o \
				read_write.o stat.o link.o misc.o exec.o device.o \
				ext2/ext2fs.o

INITFS		= initfs/initfs
PROCFS		= procfs/procfs
SYSFS		= sysfs/sysfs

FS			= fs.o

everything : realclean $(FS) $(INITFS) $(PROCFS) $(SYSFS)

realclean :
	@rm -f $(FSOBJS) $(INITFS) $(PROCFS) $(SYSFS)

$(FS): $(FSOBJS)
	@echo -e '\tLD\tfs/$@'
	@$(LD) -r -o $(FS) $(FSOBJS) $(LIBS)

%.o: %.c
	@echo -e '\tCC\tfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<

ext2/ext2fs.o:
	@(cd ext2; make)

$(INITFS):
	@(cd initfs; make)
	@(cp -f $(INITFS) $(BINDIR)/sbin/)

$(PROCFS):
	@(cd procfs; make)
	@(cp $(PROCFS) $(BINDIR)/sbin/)

$(SYSFS):
	@(cd sysfs; make)
	@(cp $(SYSFS) $(BINDIR)/sbin/)
