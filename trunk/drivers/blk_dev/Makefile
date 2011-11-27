#
# Makefile for the Lyos block device drivers.

ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I ../../include/ -I ../../include/sys/ -f elf
CFLAGS		= -I ../../include/ -I ../../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

SCSIDRV		= scsi/scsi.o
BLKDRVOBJS	= floppy.o hd.o ramdisk.o rw_blk.o $(SCSIDRV)
BLKDRV		= blk_dev.o

everything : reclean $(BLKDRV)

reclean :
	rm -f $(BLKDRVOBJS)

$(BLKDRV): $(BLKDRVOBJS)
	$(LD) -r -o $(BLKDRV) $(BLKDRVOBJS)

floppy.o: floppy.c
	$(CC) $(CFLAGS) -o $@ $<

hd.o: hd.c
	$(CC) $(CFLAGS) -o $@ $<

ramdisk.o: ramdisk.c
	$(CC) $(CFLAGS) -o $@ $<

rw_blk.o: rw_blk.c
	$(CC) $(CFLAGS) -o $@ $<

$(SCSIDRV):
	(cd scsi; make)
