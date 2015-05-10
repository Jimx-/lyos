#
# Makefile for input driver.

INPUT		= input
INPUTOBJS = main.o

LIBS	= -llyos -lsysfs

everything : realclean $(INPUT)

realclean :
	@rm -f $(INPUT) $(INPUTOBJS)

$(INPUT): $(INPUTOBJS)
	@echo -e '\tCC\tservers/input/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	@echo -e '\tCC\tservers/input/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
