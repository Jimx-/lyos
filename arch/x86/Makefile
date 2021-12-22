ASM		= nasm
DASM		   = objdump

DASMFLAGS	= -D

ifeq ($(CONFIG_X86_32),y)
	BITS := 32
else
	BITS := 64
	CFLAGS	+= -mcmodel=kernel
endif

CFLAGS += -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf$(BITS)

export ASMKFLAGS BITS
