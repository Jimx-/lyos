#
# Makefile for libsysfs.
#

LIBSYSFSOBJS		= sysfs_publish.o sysfs_retrieve.o
LIBSYSFS 		= $(LIBOUTDIR)/libsysfs.a
ARFLAGS			= rcs

everything : realclean $(LIBSYSFS)

realclean :
	@rm -f $(LIBSYSFSOBJS)

$(LIBSYSFS): $(LIBSYSFSOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libsysfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<
