#
# Makefile for the Lyos memory manager.

include arch/$(ARCH)/Makefile

SRCS	+= main.c fork.c pagetable.c alloc.c region.c vmalloc.c brk.c pagefault.c misc.c \
			mmap.c global.c utils.c slab.c vfs.c file.c cache.c region_avl.c page.c \
			file_map.c anon_map.c anon_contig_map.c direct_phys_map.c shared_map.c

LIBS	= exec lyos

CFLAGS	+= -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)
SERVERCFLAGS += -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)

EXTRACFLAGS = -Wl,-Map,mm.map

PROG	= mm

include lyos.service.mk
