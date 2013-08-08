
CC	= arm-elf-gcc
LD	= arm-elf-ld

ASMFLAGS= -O2 -g
MAKEFLAGS = --no-print-directory

OBJS	= start.o
ARMKRNL = arm-krnl.o

.c.o:
	@echo -e '\tCC\t$@'
	@$(CC) $(CFLAGS) -c $<
.s.o:
	@echo -e '\tASM\t$@'
	@$(CC) $(ASMFLAGS) -c $<

.PHONY: everything clean

everything: clean $(ARMKRNL)

$(ARMKRNL):	$(OBJS)
	@echo -e '\tLD\t$@'
	@$(LD) -r -o $@ $?
	
clean:
	@rm *.o $(ARMKRNL) -f
