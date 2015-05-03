
FSOBJS 	= main.o global.o node.o utils.o buffer.o file.o
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


