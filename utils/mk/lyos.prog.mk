
SRC_PATH = .
BUILD_PATH = ./obj.$(SUBARCH)
OBJS = $(patsubst %.c, $(BUILD_PATH)/%.o, $(patsubst %.S, $(BUILD_PATH)/%.o, $(patsubst %.asm, $(BUILD_PATH)/%.o, $(SRCS))))
OBJDIRS = $(sort $(patsubst %/,%,$(dir $(OBJS) $(BIN))))
LINKLIBS = $(LIBS:%=$(LIBOUTDIR)/lib%.a)
DEPS = $(OBJS:.o=.d)

INSTALL_PREFIX ?= /bin

BIN = $(BUILD_PATH)/$(PROG)

.PHONY : everything all clean realclean install

all : $(BIN)
	@true

everything : $(BIN)
	@true

clean :
	$(Q)rm -f $(BIN)

realclean :
	$(Q)rm -f $(BIN) $(OBJS)

install :
	$(Q)$(INSTALL) $(BIN) $(DESTDIR)$(INSTALL_PREFIX)

$(BIN): $(OBJS) $(LINKLIBS) $(EXTRAOBJS)
	@echo -e '\tLD\t$(PROG)/$@'
	$(Q)$(CC) $(CFLAGS) $(EXTRACFLAGS) -o $@ $(OBJS) $(EXTRAOBJS) $(EXTRALIBS) $(patsubst %,-l%,$(LIBS))

$(OBJDIRS):
	$(Q)mkdir -p $@

$(OBJS) $(BIN): | $(OBJDIRS)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\t$(PROG)/$<'
	$(Q)$(CC) $(CFLAGS) -MP -MMD -c $< -o $@
