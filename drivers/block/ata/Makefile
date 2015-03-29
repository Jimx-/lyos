
ATA 	= ata
ATAOBJS 	= main.o
LIBS	= -llyos -ldriver

everything : realclean $(ATA)

all : realclean $(ATA)

realclean :
	@rm -f $(ATA) $(ATAOBJS)

$(ATA): $(ATAOBJS)
	@echo -e '\tLD\tdrivers/block/ata/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) 

%.o: %.c
	@echo -e '\tCC\tdrivers/block/ata/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
