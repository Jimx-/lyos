
RDOBJS 	= main.o
LIBS	= -lblockdriver -lchardriver -llyos 

RAMDISK = ramdisk

everything : realclean $(RAMDISK)

all : realclean $(RAMDISK)

realclean :
	@rm -f $(RAMDISK) $(RDOBJS)

$(RAMDISK): $(RDOBJS)
	@echo -e '\tLD\tdrivers/block/ramdisk/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)  -Xlinker -Ttext -Xlinker 0x60000000

%.o: %.c
	@echo -e '\tCC\tdrivers/block/ramdisk/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
