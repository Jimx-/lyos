#
# Makefile for the Lyos block device drivers
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

SCSIDRV		= scsi/scsi.o
BLKDRVOBJS	= floppy.o hd.o ramdisk.o rw_blk.o $(SCSIDRV)
BLKDRV		= blk_dev.o

everything : realclean $(BLKDRV)

realclean :
	@rm -f $(BLKDRVOBJS)

$(BLKDRV): $(BLKDRVOBJS)
	@echo -e '\tLD\tdrivers/blk_dev/$@'
	@$(LD) -r -o $(BLKDRV) $(BLKDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/blk_dev/$@'
	@$(CC) $(CFLAGS) -o $@ $<

$(SCSIDRV):
	@(cd scsi; make)
