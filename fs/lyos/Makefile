
CFLAGS	= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

FSOBJS 	= main.o open.o stat.o read_write.o\
			link.o \
			disklog.o buffer.o file.o namei.o

LYOSFS	= lyosfs.o

everything : reclean $(LYOSFS)

all : reclean $(LYOSFS)

reclean :
	@rm -f $(FSOBJS)

$(LYOSFS): $(FSOBJS)
	@echo -e '\tLD\tfs/lyos/$@'
	@$(LD) -r -o lyosfs.o $(FSOBJS)

%.o: %.c
	@echo -e '\tCC\tfs/lyos/$@'
	@$(CC) $(CFLAGS) -o $@ $<

