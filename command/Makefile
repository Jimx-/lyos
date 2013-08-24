# commands/Makefile

#ENTRYPOINT	= 0x1000
HD		= ../80m.img

ASM		= nasm
DASM		= objdump
CC		= gcc
LD		= ld
ASMFLAGS	= -I ../include/ -f elf
LDFLAGS		= -Ttext 0x1000
DASMFLAGS	= -D
LIB		= ../lib/liblyos.a
BIN		= sh echo pwd rm touch date cat su uname ls cp

# All Phony Targets
.PHONY : everything final clean realclean disasm all install

# Default starting position
everything : $(BIN)

install : all clean
	@echo -e '\tINSTALL'
	@cp ../arch/x86/boot/hd*.bin ./
	@cp ../arch/x86/lyos.bin ./
	@tar cf cmd.tar lyos.bin $(BIN) hd*.bin hello.c
	dd if=cmd.tar of=$(HD) seek=`echo "obase=10;ibase=16;(\`egrep -e '^ROOT_BASE' ../arch/x86/boot/include/load.inc | sed -e 's/.*0x//g'\`+\`egrep -e '#define[[:space:]]*INSTALL_START_SECT' ../include/lyos/config.h | sed -e 's/.*0x//g'\`)*200" | bc` bs=1 count=`ls -l cmd.tar | awk -F " " '{print $$5}'` conv=notrunc

all : realclean everything

final : all clean

clean :
	@rm -f *.o

realclean :
	@rm -f $(BIN) *.o

lyos.bin :
	@cp ../lyos.bin ./

start.o : start.asm
	@echo -e '\tASM\tcommand/$@'
	@$(ASM) $(ASMFLAGS) -o $@ $<

sh.o: sh.c ../include/stdio.h ../include/lyos/type.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

sh : sh.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

echo.o: echo.c ../include/lyos/type.h ../include/stdio.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

echo : echo.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

pwd.o: pwd.c ../include/lyos/type.h ../include/stdio.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

pwd : pwd.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

rm.o: rm.c ../include/lyos/type.h ../include/stdio.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

rm : rm.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

touch.o: touch.c ../include/lyos/type.h ../include/stdio.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

touch : touch.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

date.o: date.c ../include/lyos/type.h ../include/stdio.h ../include/lyos/const.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

date : date.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

cat.o: cat.c ../include/stdio.h ../include/lyos/console.h 
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

cat : cat.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

su.o: su.c ../include/stdio.h ../include/lyos/type.h 
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

su : su.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

uname.o: uname.c ../include/stdio.h ../include/lyos/type.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

uname : uname.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

ls.o: ls.c ../include/stdio.h ../include/lyos/type.h ../include/lyos/fs.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

ls : ls.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?
	
cp.o: cp.c ../include/stdio.h ../include/lyos/type.h ../include/lyos/fs.h ../include/lyos/const.h
	@echo -e '\tCC\tcommand/$@'
	@$(CC) $(CFLAGS) -o $@ $<

cp : cp.o start.o $(LIB)
	@echo -e '\tLD\tcommand/$@'
	@$(LD) $(LDFLAGS) -o $@ $?

