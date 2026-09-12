###############################
# Makefile for vmx-smoke test #
###############################

PROG	= vmx-smoke

SRCS	= vmx-smoke.c guest.S

LIBS	= lyos

include lyos.prog.mk

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.S
	@echo -e '\tAS\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
