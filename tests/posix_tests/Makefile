SRCS	= main.c pipe.c eventfd.c signalfd.c timerfd.c epoll.c uds.c dl.c mmap.c netlink.c \
			inotify.c pty.c
PROG	= posix_tests

CFLAGS  = -I..

EXTRAOBJS = $(BUILD_PATH)/munit.o
EXTRALIBS = -ldl

include lyos.prog.mk

$(BUILD_PATH)/munit.o: $(SRC_PATH)/../munit/munit.c
	@echo -e '\tCC\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
