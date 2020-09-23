SRCS	= main.c pipe.c eventfd.c
PROG	= posix_tests

CFLAGS  = -I..

EXTRAOBJS = $(BUILD_PATH)/munit.o

include lyos.prog.mk

$(BUILD_PATH)/munit.o: $(SRC_PATH)/../munit/munit.c
	@echo -e '\tCC\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
