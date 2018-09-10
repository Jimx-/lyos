ASM		= nasm
DASM	       = objdump

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

export ASMKFLAGS

DASMFLAGS	= -D

ifeq ($(CONFIG_X86_32),y)
       BITS := 32
else
       BITS := 64
endif

export BITS
