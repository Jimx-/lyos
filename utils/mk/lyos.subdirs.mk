

.PHONY: everything all clean realclean install subdirs $(SUBDIRS)

all: subdirs

subdirs: $(SUBDIRS)

everything: subdirs

clean: subdirs

realclean: subdirs

install: subdirs

$(SUBDIRS):
	$(Q)$(MAKE) -C $@ $(MAKEFLAGS) $(MAKECMDGOALS)
