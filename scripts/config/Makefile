CFLAGS = -c

CONFBIN = conf
MCONFBIN = mconf

config: clean $(CONFBIN)

menuconfig: clean dochecklxdialog $(MCONFBIN)

check-lxdialog  = lxdialog/check-lxdialog.sh

lxdialog = lxdialog/checklist.o lxdialog/util.o lxdialog/inputbox.o
lxdialog += lxdialog/textbox.o lxdialog/yesno.o lxdialog/menubox.o

EXTRACFLAGS = $(shell $(check-lxdialog) -ccflags)
LOADLIBES   = $(shell $(check-lxdialog) -ldflags $(HOSTCC))
EXTRACFLAGS += -DLOCALE

dochecklxdialog:
	@echo "\tCHK\tlxdialog"
	@$(check-lxdialog) -check $(HOSTCC) $(EXTRACFLAGS) $(LOADLIBES)

conf-objs	:= conf.o  zconf.tab.o
mconf-objs     := mconf.o zconf.tab.o $(lxdialog)

$(CONFBIN):  $(conf-objs)
	@echo "\tHOSTCC\t$@"
	@$(HOSTCC) -o $@ $^

$(MCONFBIN): $(mconf-objs)
	@echo "\tHOSTCC\t$@"
	@$(HOSTCC) $^ $(LOADLIBES) -o $@

clean-files	:= zconf.tab.c zconf.lex.c zconf.hash.c $(lxdialog)
clean-files     += conf mconf

clean:
	@rm -f $(clean-files)

%.o: %.c
	@echo "\tHOSTCC\t$@"
	@$(HOSTCC) $(EXTRACFLAGS) -c $< -o $@

zconf.tab.o: zconf.tab.c zconf.lex.c zconf.hash.c
	@echo "\tHOSTCC\t$@"
	@$(HOSTCC) $(CFLAGS) -o $@ $<

%.c: %.c_shipped
	@echo "\tSHIPPED\t$@"
	@cat $< > $@
