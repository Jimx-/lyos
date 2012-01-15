CC	= gcc
CFLAGS	= -I ../../include/ -I ../../include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
LD	= ld

FSOBJS 	= main.o open.o misc.o read_write.o\
			link.o \
			disklog.o buffer.o file.o namei.o

LYOSFS	= lyosfs.o

everything : reclean $(LYOSFS)

all : reclean $(LYOSFS)

reclean :
	rm -f $(FSOBJS)

$(LYOSFS): $(FSOBJS)
	$(LD) -r -o lyosfs.o $(FSOBJS)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

open.o: open.c
	$(CC) $(CFLAGS) -o $@ $<

read_write.o: read_write.c
	$(CC) $(CFLAGS) -o $@ $<

link.o: link.c
	$(CC) $(CFLAGS) -o $@ $<

disklog.o: disklog.c
	$(CC) $(CFLAGS) -o $@ $<

buffer.o: buffer.c
	$(CC) $(CFLAGS) -o $@ $<

file.o: file.c
	$(CC) $(CFLAGS) -o $@ $<

namei.o: namei.c
	$(CC) $(CFLAGS) -o $@ $<
