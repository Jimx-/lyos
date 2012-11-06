#
# Makefile for the Lyos block device drivers
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

SCSIDRV		= scsi/scsi.o
BLKDRVOBJS	= floppy.o hd.o ramdisk.o rw_blk.o $(SCSIDRV)
BLKDRV		= blk_dev.o

everything : reclean $(BLKDRV)

reclean :
	@rm -f $(BLKDRVOBJS)

$(BLKDRV): $(BLKDRVOBJS)
	@echo -e '\tLD\tdrivers/blk_dev/$@'
	@$(LD) -r -o $(BLKDRV) $(BLKDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/blk_dev/$@'
	@$(CC) $(CFLAGS) -o $@ $<

$(SCSIDRV):
	@(cd scsi; make)
