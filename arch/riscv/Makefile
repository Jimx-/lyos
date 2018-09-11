
ifeq ($(CONFIG_ARCH_RV64I),y)
	BITS := 64

	MARCH = rv64im
	LDFLAGS += -melf64lriscv
else
	BITS := 32

	MARCH = rv32im
	LDFLAGS += -melf32lriscv
endif

export BITS

ifeq ($(CONFIG_RISCV_ISA_A),y)
	ARCH_A = a
endif
ifeq ($(CONFIG_RISCV_ISA_C),y)
	ARCH_C = c
endif

CFLAGS += -mno-save-restore
SERVERCFLAGS += -mno-save-restore

ifeq ($(CONFIG_CMODEL_MEDLOW),y)
	CFLAGS += -mcmodel=medlow
endif

ifeq ($(CONFIG_CMODEL_MEDANY),y)
	CFLAGS += -mcmodel=medany
endif
