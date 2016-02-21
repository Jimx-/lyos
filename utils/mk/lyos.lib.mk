
SRC_PATH = .
BUILD_PATH = ./obj
OBJS = $(patsubst %.c, $(BUILD_PATH)/%.o, $(patsubst %.S, $(BUILD_PATH)/%.o, $(patsubst %.asm, $(BUILD_PATH)/%.o, $(SRCS))))
DEPS = $(OBJS:.o=.d)
INSTALL_PREFIX = /lib

ifeq ($(wildcard arch),) 
	ARCH_BUILD_PATH = 
else
	ARCH_BUILD_PATH = $(BUILD_PATH)/arch/$(ARCH)
endif

BIN = $(BUILD_PATH)/lib$(LIB).a

.PHONY : everything all clean realclean install

all : $(BUILD_PATH) $(ARCH_BUILD_PATH) $(BIN)
	@true

everything : $(BUILD_PATH) $(ARCH_BUILD_PATH) $(BIN)
	@true

clean :
	$(Q)rm -f $(BIN)

realclean :
	$(Q)rm -f $(BIN) $(OBJS)

install :
	$(Q)$(INSTALL) $(BIN) $(DESTDIR)$(INSTALL_PREFIX)

$(BIN): $(OBJS) $(EXTRAOBJS)
	@echo -e '\tAR\tlib$(LIB)/$@'
	$(Q)$(AR) $(ARFLAGS) $@ $(OBJS) $(EXTRAOBJS)

$(BUILD_PATH):
	$(Q)mkdir $(BUILD_PATH)

$(BUILD_PATH)/arch/$(ARCH):
	$(Q)mkdir -p $(BUILD_PATH)/arch/$(ARCH)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\tlib$(LIB)/$<'
	$(Q)$(CC) $(CFLAGS) -MP -MMD $< -o $@

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.asm
	@echo -e '\tASM\tlib$(LIB)/$<'
	$(Q)$(ASM) $(ASMKFLAGS) -o $@ $<

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.S
	@echo -e '\tAS\tlib$(LIB)/$<'
	$(Q)$(AS) $(ASFLAGS) -o $@ $<
