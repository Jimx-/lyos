ASM		= nasm
ASMKFLAGS	= -I ../include/ -I ../include/sys/ -f elf
CC		= gcc
CFLAGS		= -I ../include/ -I ../include/sys/ -c -fno-builtin -Wall
LD			= ld
LDFLAGS		= -Ttext $(ENTRYPOINT) -Map ../krnl.map

ENTRYPOINT	= 0x1000

LYOSKERNEL	= ../kernel.bin

OBJS		= kernel.o start.o main.o\
			clock.o keyboard.o tty.o console.o\
			i8259.o global.o protect.o proc.o\
			systask.o blk_dev/hd.o\
			kliba.o klib.o sys.o signal.o \
			reboot.o

MMOBJS =	../mm/main.o ../mm/forkexit.o ../mm/exec.o ../mm/page.o ../mm/memory.o 

FSOBJS =	../fs/main.o ../fs/open.o ../fs/misc.o ../fs/read_write.o\
			../fs/link.o \
			../fs/disklog.o 

LIB			=   ../lib/syscall.o\
			../lib/printf.o ../lib/vsprintf.o\
			../lib/getpid.o ../lib/stat.o ../lib/string.o ../lib/misc.o\
			../lib/open.o ../lib/read.o ../lib/write.o ../lib/close.o\
			../lib/unlink.o\
			../lib/lseek.o\
			../lib/fork.o ../lib/exit.o ../lib/wait.o ../lib/exec.o

everything : reclean $(LYOSKERNEL)

reclean :
	rm -f $(OBJS)

$(LYOSKERNEL) : $(OBJS) $(LIB) $(MMOBJS) $(FSOBJS)
	$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^

kernel.o : kernel.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

reboot.o: reboot.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

start.o: start.c
	$(CC) $(CFLAGS) -o $@ $<

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

clock.o: clock.c
	$(CC) $(CFLAGS) -o $@ $<

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) -o $@ $<

tty.o: tty.c
	$(CC) $(CFLAGS) -o $@ $<

console.o: console.c
	$(CC) $(CFLAGS) -o $@ $<

i8259.o: i8259.c
	$(CC) $(CFLAGS) -o $@ $<

global.o: global.c
	$(CC) $(CFLAGS) -o $@ $<

protect.o: protect.c
	$(CC) $(CFLAGS) -o $@ $<

proc.o: proc.c
	$(CC) $(CFLAGS) -o $@ $<

systask.o: systask.c
	$(CC) $(CFLAGS) -o $@ $<

blk_dev/hd.o: blk_dev/hd.c
	$(CC) $(CFLAGS) -o $@ $<

klib.o: klib.c
	$(CC) $(CFLAGS) -o $@ $<

sys.o: sys.c
	$(CC) $(CFLAGS) -o $@ $<

signal.o: signal.c
	$(CC) $(CFLAGS) -o $@ $<

kliba.o : kliba.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<
