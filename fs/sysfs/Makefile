
FSOBJS 	= main.o global.o node.o utils.o buffer.o file.o
LIBS	= -lmemfs -lfsdriver -ldriver -llyos

SYSFS = sysfs

everything : realclean $(SYSFS)

all : realclean $(SYSFS)

realclean :
	@rm -f $(SYSFS) $(FSOBJS)

$(SYSFS): $(FSOBJS)
	@echo -e '\tLD\tfs/sysfs/$@'
	@$(CC) $(SERVERCFLAGS) -Wl,-Ttext,0x80000000 -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tfs/sysfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


