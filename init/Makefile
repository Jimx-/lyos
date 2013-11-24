##############################
# Makefile for Lyos init.	 #
##############################

INIT		= init

everything : realclean $(INIT)

realclean :
	@rm -f $(INIT)

$(INIT): main.c
	@echo -e '\tCC\tinit/$@'
	@$(CC) -o $@ $< -O2 -Xlinker -Map -Xlinker init.map
	@cp $(INIT) $(SRCDIR)/sysroot/sbin/
