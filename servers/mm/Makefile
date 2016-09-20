#
# Makefile for the Lyos memory manager.

SRCS 	= main.c fork.c pagetable.c alloc.c region.c vmalloc.c brk.c \
			pagefault.c misc.c mmap.c global.c utils.c slab.c vfs.c file.c cache.c futex.c
LIBS 	= exec lyos

PROG	= mm
SERVERCFLAGS  += -Xlinker -Ttext -Xlinker 0x70000000

include lyos.service.mk
