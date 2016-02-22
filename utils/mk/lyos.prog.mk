
SRC_PATH = .
BUILD_PATH = ./obj
OBJS = $(patsubst %.c, $(BUILD_PATH)/%.o, $(patsubst %.S, $(BUILD_PATH)/%.o, $(patsubst %.asm, $(BUILD_PATH)/%.o, $(SRCS))))
LINKLIBS = $(LIBS:%=$(LIBOUTDIR)/lib%.a)
DEPS = $(OBJS:.o=.d)

BIN = $(BUILD_PATH)/$(PROG)

ifeq ($(wildcard arch),) 
	ARCH_BUILD_PATH = 
else
	ARCH_BUILD_PATH = $(BUILD_PATH)/arch/$(ARCH)
endif

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

$(BIN): $(OBJS) $(LINKLIBS) $(EXTRAOBJS)
	@echo -e '\tLD\t$(PROG)/$@'
	$(Q)$(CC) $(CFLAGS) $(EXTRACFLAGS) -o $@ $(OBJS) $(EXTRAOBJS) $(patsubst %,-l%,$(LIBS))

$(BUILD_PATH):
	$(Q)mkdir $(BUILD_PATH)

$(BUILD_PATH)/arch/$(ARCH):
	$(Q)mkdir -p $(BUILD_PATH)/arch/$(ARCH)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -MP -MMD -c $< -o $@
