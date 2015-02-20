#
# Makefile for the Lyos filesystems.

INITFS		= initfs/initfs
PROCFS		= procfs/procfs
SYSFS		= sysfs/sysfs
EXT2FS 		= ext2/ext2fs

everything : realclean $(INITFS) $(PROCFS) $(SYSFS) $(EXT2FS)

realclean :
	@rm -f $(INITFS) $(PROCFS) $(SYSFS) $(EXT2FS)

$(EXT2FS):
	@(cd ext2; make)
	@(cp -f $(EXT2FS) $(DESTDIR)/sbin/)

$(INITFS):
	@(cd initfs; make)
	@(cp -f $(INITFS) $(DESTDIR)/sbin/)

$(PROCFS):
	@(cd procfs; make)
	@(cp $(PROCFS) $(DESTDIR)/sbin/)

$(SYSFS):
	@(cd sysfs; make)
	@(cp $(SYSFS) $(DESTDIR)/sbin/)
