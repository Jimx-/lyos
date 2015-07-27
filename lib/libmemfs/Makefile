#
# Makefile for libmemfs.
#

LIBMEMFSOBJS	= memfs.o super.o inode.o lookup.o protect.o stat.o readwrite.o getdents.o
LIBMEMFS 		= $(LIBOUTDIR)/libmemfs.a
ARFLAGS			= rcs

everything : realclean $(LIBMEMFS)

realclean :
	@rm -f $(LIBMEMFSOBJS)

$(LIBMEMFS): $(LIBMEMFSOBJS)
	@echo -e '\tAR\t$@'
	@$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo -e '\tCC\tlib/libmemfs/$@'
	@$(CC) $(CFLAGS) -o $@ $<
