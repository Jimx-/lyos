SRCS	= main.c pipe.c eventfd.c signalfd.c timerfd.c
PROG	= posix_tests

CFLAGS  = -I..

EXTRAOBJS = $(BUILD_PATH)/munit.o

include lyos.prog.mk

$(BUILD_PATH)/munit.o: $(SRC_PATH)/../munit/munit.c
	@echo -e '\tCC\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
