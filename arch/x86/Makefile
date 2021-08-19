ASM		= nasm
DASM		   = objdump

DASMFLAGS	= -D

ifeq ($(CONFIG_X86_32),y)
	BITS := 32
else
	BITS := 64
	CFLAGS	+= -mcmodel=kernel
endif

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf$(BITS)

export ASMKFLAGS BITS
