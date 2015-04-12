
.PHONY: everything login-utils sys-utils cmd-utils

everything : realclean login-utils sys-utils cmd-utils

realclean:
	@(cd sys-utils; make realclean)
	@(cd login-utils; make realclean)
	@(cd cmd-utils; make realclean)

sys-utils:
	@(cd sys-utils; make)

login-utils:
	@(cd login-utils; make)

cmd-utils:
	@(cd cmd-utils; make)
