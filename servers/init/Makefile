##############################
# Makefile for Lyos init.	 #
##############################

INIT		= init

everything : realclean $(INIT)

realclean :
	@rm -f $(INIT)

$(INIT): main.c
	@echo -e '\tCC\tservers/init/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $<
