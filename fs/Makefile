#
# Makefile for the Lyos filesystem.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

FSOBJS		= main.o super.o lyos/lyosfs.o
FS			= fs.o

everything : reclean $(FS)

reclean :
	@rm -f $(FSOBJS)

$(FS): $(FSOBJS)
	@echo -e '\tLD\tfs/$@'
	@$(LD) -r -o $(FS) $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<

lyos/lyosfs.o:
	@(cd lyos; make)

