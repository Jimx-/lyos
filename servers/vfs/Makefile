#
# Makefile for the Lyos filesystem.

SRCS	= main.c super.c open.c mount.c global.c path.c inode.c protect.c \
			read_write.c stat.c link.c misc.c exec.c device.c file.c \
			cdev.c select.c worker.c ipc.c pipe.c eventfd.c anon_inodes.c \
			signalfd.c wait_queue.c timerfd.c eventpoll.c driver.c sdev.c \
			socket.c lock.c

LIBS	= exec lyos devman coro sysfs

PROG	= vfs

EXTRACFLAGS = -Xlinker -Ttext -Xlinker 0x60000000 -Wl,-Map,vfs.map -g

include lyos.service.mk
