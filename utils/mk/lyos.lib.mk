
SRC_PATH = .
BUILD_PATH = ./obj
OBJS = $(SRCS:%.c=$(BUILD_PATH)/%.o)
DEPS = $(OBJS:.o=.d)
INSTALL_PREFIX = /lib

BIN = $(BUILD_PATH)/lib$(LIB).a

.PHONY : everything all clean realclean install

all : $(BUILD_PATH) $(BIN)

everything : $(BUILD_PATH) $(BIN)

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

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\tlib$(LIB)/$<'
	$(Q)$(CC) $(CFLAGS) -MP -MMD $< -o $@
