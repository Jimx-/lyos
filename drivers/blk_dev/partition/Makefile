
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

OBJ		= partition.o

everything : realclean $(OBJ)

realclean :
	@rm -f $(OBJ)

$(OBJ): partition.c
	@echo -e '\tCC\tdrivers/blk_dev/partition/$@'
	@$(CC) $(CFLAGS) -o $@ $<


