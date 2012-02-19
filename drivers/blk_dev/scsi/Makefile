#
# Makefile for scsi driver

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

SCSI		= scsi.o

everything : reclean $(SCSI)

reclean :
	@rm -f $(SCSI)

$(SCSI): scsi.c
	@echo -e '\tCC\tdrivers/blk_dev/scsi/$@'
	@$(CC) $(CFLAGS) -o $@ $<


