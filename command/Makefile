# commands/Makefile

#ENTRYPOINT	= 0x1000
HD		= ../80m.img

ASM		= nasm
DASM		= objdump
CC		= gcc
LD		= ld
ASMFLAGS	= -I ../include/ -f elf
CFLAGS		= -I ../include/ -c -fno-builtin -fno-stack-protector -Wall
LDFLAGS		= -Ttext 0x1000
DASMFLAGS	= -D
LIB		= ../lib/Lyoscrt.a
BIN		= sh echo pwd rm touch date cat su uname ls cp

# All Phony Targets
.PHONY : everything final clean realclean disasm all install

# Default starting position
everything : $(BIN)

install : all clean
	cp ../boot/hd*.bin ./ -v
	cp ../kernel.bin ./ -v
	tar vcf cmd.tar kernel.bin $(BIN) hd*.bin hello.c
	dd if=cmd.tar of=$(HD) seek=`echo "obase=10;ibase=16;(\`egrep -e '^ROOT_BASE' ../boot/include/load.inc | sed -e 's/.*0x//g'\`+\`egrep -e '#define[[:space:]]*INSTALL_START_SECT' ../include/sys/config.h | sed -e 's/.*0x//g'\`)*200" | bc` bs=1 count=`ls -l cmd.tar | awk -F " " '{print $$5}'` conv=notrunc

all : realclean everything

final : all clean

clean :
	rm -f *.o

realclean :
	rm -f $(BIN) *.o

kernel.bin :
	cp ../kernel.bin ./

start.o : start.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

sh.o: sh.c ../include/stdio.h ../include/type.h
	$(CC) $(CFLAGS) -o $@ $<

sh : sh.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

echo.o: echo.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

echo : echo.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

pwd.o: pwd.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

pwd : pwd.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

rm.o: rm.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

rm : rm.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

touch.o: touch.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

touch : touch.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

date.o: date.c ../include/type.h ../include/stdio.h ../include/sys/const.h
	$(CC) $(CFLAGS) -o $@ $<

date : date.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

cat.o: cat.c ../include/stdio.h ../include/sys/console.h 
	$(CC) $(CFLAGS) -o $@ $<

cat : cat.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

su.o: su.c ../include/stdio.h ../include/type.h 
	$(CC) $(CFLAGS) -o $@ $<

su : su.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

uname.o: uname.c ../include/stdio.h ../include/type.h
	$(CC) $(CFLAGS) -o $@ $<

uname : uname.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

ls.o: ls.c ../include/stdio.h ../include/type.h ../include/sys/fs.h
	$(CC) $(CFLAGS) -o $@ $<

ls : ls.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?
	
cp.o: cp.c ../include/stdio.h ../include/type.h ../include/sys/fs.h ../include/sys/const.h
	$(CC) $(CFLAGS) -o $@ $<

cp : cp.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?
