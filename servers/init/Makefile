##############################
# Makefile for Lyos init.	 #
##############################

INITOBJS 	= main.o 
INIT		= init

everything : realclean $(INIT)

realclean :
	@rm -f $(INIT) $(INITOBJS)

$(INIT): $(INITOBJS)
	@echo -e '\tLD\tservers/init/$@'
	@$(CC) -o $@ $^

%.o: %.c
	@echo -e '\tCC\tservers/init/$@'
	@$(CC) -c $<
