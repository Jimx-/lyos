#
# Makefile for the Lyos block device drivers
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

BLKDRVOBJS	= hd.o
BLKDRV		= block.o

RAMDISK		= ramdisk/ramdisk

everything : realclean $(BLKDRV) $(RAMDISK)

realclean :
	@rm -f $(BLKDRVOBJS) $(RAMDISK)

$(BLKDRV): $(BLKDRVOBJS)
	@echo -e '\tLD\tdrivers/block/$@'
	@$(LD) -r -o $(BLKDRV) $(BLKDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/block/$@'
	@$(CC) $(CFLAGS) -o $@ $<

$(RAMDISK):
	@(cd ramdisk; make)
	@(cp $(RAMDISK) $(BINDIR)/sbin)
