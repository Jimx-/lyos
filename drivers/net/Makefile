#
# Makefile for inet driver.

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

INET		= inet.o

everything : reclean $(INET)

reclean :
	@rm -f $(INET)

$(INET): inet.c
	@echo -e '\tCC\tdrivers/net/$@'
	@$(CC) $(CFLAGS) -o $@ $<


