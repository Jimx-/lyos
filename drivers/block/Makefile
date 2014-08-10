#
# Makefile for the Lyos block device drivers
#

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

BLKDRVOBJS	= floppy.o hd.o ramdisk.o
BLKDRV		= block.o

everything : realclean $(BLKDRV)

realclean :
	@rm -f $(BLKDRVOBJS)

$(BLKDRV): $(BLKDRVOBJS)
	@echo -e '\tLD\tdrivers/block/$@'
	@$(LD) -r -o $(BLKDRV) $(BLKDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/block/$@'
	@$(CC) $(CFLAGS) -o $@ $<
