
.PHONY: everything login-utils sys-utils

everything : realclean login-utils sys-utils

realclean:
	@(cd sys-utils; make realclean)
	@(cd login-utils; make realclean)

sys-utils:
	@(cd sys-utils; make)

login-utils:
	@(cd login-utils; make)
