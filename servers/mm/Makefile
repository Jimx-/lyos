#
# Makefile for the Lyos memory manager.

SRCS	= main.c fork.c pagetable.c alloc.c region.c vmalloc.c brk.c \
			pagefault.c misc.c mmap.c global.c utils.c slab.c vfs.c file.c cache.c futex.c region_avl.c

LIBS	= exec lyos

CFLAGS	+= -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)
SERVERCFLAGS += -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)

PROG	= mm

include lyos.service.mk
