
ASM		= nasm
CC		= gcc
LD		= ld
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

OBJ		= partition.o

everything : reclean $(OBJ)

reclean :
	rm -f $(OBJ)

$(OBJ): partition.c
	$(CC) $(CFLAGS) -o $@ $<


