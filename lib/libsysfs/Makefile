#
# Makefile for libsysfs.
#

LIBSYSFSOBJS		= sysfs_publish.o
LIBSYSFS 		= libsysfs.a
ARFLAGS			= rcs

everything : realclean $(LIBSYSFS)

realclean :
	@rm -f $(LIBSYSFSOBJS)

$(LIBSYSFS): $(LIBSYSFSOBJS)
	@echo -e '\tAR\tlib/libsysfs/$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libsysfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<
