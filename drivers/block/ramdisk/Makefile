
RDOBJS 	= main.o
LIBS	= -llyos -ldriver

RAMDISK = ramdisk

everything : realclean $(RAMDISK)

all : realclean $(RAMDISK)

realclean :
	@rm -f $(RAMDISK) $(RDOBJS)

$(RAMDISK): $(RDOBJS)
	@echo -e '\tLD\tdrivers/block/ramdisk/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tdrivers/block/ramdisk/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
