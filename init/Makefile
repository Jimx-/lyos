##############################
# Makefile for Lyos init.	 #
##############################

INIT		= init

everything : realclean $(INIT)

realclean :
	@rm -f $(INIT)

$(INIT): main.c
	@echo -e '\tCC\tinit/$@'
	@$(CC) -o $@ $<
	@cp $(INIT) $(SRCDIR)/sysroot/sbin/
