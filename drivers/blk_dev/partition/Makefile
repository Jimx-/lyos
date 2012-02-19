
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

OBJ		= partition.o

everything : reclean $(OBJ)

reclean :
	@rm -f $(OBJ)

$(OBJ): partition.c
	@echo -e '\tCC\tdrivers/blk_dev/partition/$@'
	@$(CC) $(CFLAGS) -o $@ $<


