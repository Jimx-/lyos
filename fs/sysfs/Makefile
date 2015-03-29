
FSOBJS 	= main.o global.o node.o publish.o
LIBS	= -lmemfs -lfsdriver -llyos -ldriver

SYSFS = sysfs

everything : realclean $(SYSFS)

all : realclean $(SYSFS)

realclean :
	@rm -f $(SYSFS) $(FSOBJS)

$(SYSFS): $(FSOBJS)
	@echo -e '\tLD\tfs/sysfs/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) 

%.o: %.c
	@echo -e '\tCC\tfs/sysfs/$@'
	@$(CC) $(SERVERCFLAGS) -c $<


