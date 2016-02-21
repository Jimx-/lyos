
SRCS 	= main.c super.c global.c inode.c protect.c path.c read_write.c buffer.c \
			alloc.c stat.c open.c link.c
LIBS	=  fsdriver bdev lyos

PROG	= ext2fs

EXTRACFLAGS = -Xlinker -Ttext -Xlinker 0x30000000

include lyos.service.mk
