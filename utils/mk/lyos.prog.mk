
SRC_PATH = .
BUILD_PATH = ./obj
OBJS = $(SRCS:%.c=$(BUILD_PATH)/%.o)
LINKLIBS = $(LIBS:%=$(LIBOUTDIR)/lib%.a)
DEPS = $(OBJS:.o=.d)

BIN = $(BUILD_PATH)/$(PROG)

.PHONY : everything all clean realclean install

all : $(BUILD_PATH) $(BIN)

everything : $(BUILD_PATH) $(BIN)

clean :
	@rm -f $(BIN)

realclean :
	@rm -f $(BIN) $(OBJS)

install :
	@$(INSTALL) $(BIN) $(DESTDIR)$(INSTALL_PREFIX)

$(BIN): $(OBJS) $(LINKLIBS) $(EXTRAOBJS)
	@echo -e '\tLD\t$(PROG)/$@'
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTRAOBJS) $(patsubst %,-l%,$(LIBS))

$(BUILD_PATH):
	@mkdir $(BUILD_PATH)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\t$(PROG)/$@'
	@$(CC) $(CFLAGS) -MP -MMD -c $< -o $@
