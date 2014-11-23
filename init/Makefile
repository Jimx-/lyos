##############################
# Makefile for Lyos init.	 #
##############################

INIT		= init

everything : realclean $(INIT)

realclean :
	@rm -f $(INIT)

$(INIT): main.c
	@echo -e '\tCC\tinit/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $< -Xlinker -Map -Xlinker init.map
	@cp $(INIT) $(SRCDIR)/sysroot/sbin/
