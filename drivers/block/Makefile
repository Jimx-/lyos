#
# Makefile for the Lyos block device drivers
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

RAMDISK		= ramdisk/ramdisk
ATA = ata/ata

everything : realclean $(RAMDISK) $(ATA)

realclean :
	@rm -f $(RAMDISK) $(ATA)

$(RAMDISK):
	@(cd ramdisk; make)
	@(cp $(RAMDISK) $(DESTDIR)/sbin)

$(ATA):
	@(cd ata; make)
	@(cp $(ATA) $(DESTDIR)/sbin)
