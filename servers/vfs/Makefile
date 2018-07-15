#
# Makefile for the Lyos filesystem.

SRCS	= main.c super.c open.c mount.c global.c path.c inode.c protect.c \
			read_write.c stat.c link.c misc.c exec.c device.c thread.c file.c \
			rwlock.c cdev.c select.c
LIBS 	= exec lyos devman pthread

PROG 	= vfs

EXTRACFLAGS = -Xlinker -Ttext -Xlinker 0x60000000 -Wl,-Map,vfs.map -g

include lyos.service.mk
