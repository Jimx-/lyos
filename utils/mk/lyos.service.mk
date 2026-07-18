
CFLAGS = $(SERVERCFLAGS)
INSTALL_PREFIX = /sbin

SRCS += $(foreach subdir,$(SUBDIRS-y),$(wildcard $(subdir)/*.c))

include lyos.prog.mk
