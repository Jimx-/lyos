#
# Makefile for scsi driver

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

SCSI		= scsi.o

everything : realclean $(SCSI)

realclean :
	@rm -f $(SCSI)

$(SCSI): scsi.c
	@echo -e '\tCC\tdrivers/block/scsi/$@'
	@$(CC) $(CFLAGS) -o $@ $<


