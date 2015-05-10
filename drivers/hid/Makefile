#
# Makefile for the Lyos HID device drivers

KEYBOARD = keyboard/keyboard

everything : realclean $(KEYBOARD)

realclean :
	@rm -f $(KEYBOARD)

$(KEYBOARD):
	@(cd keyboard; make)
	@(cp $(KEYBOARD) $(DESTDIR)/sbin)
