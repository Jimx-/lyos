##############################
# Makefile for Lyos library. #
##############################

include arch/$(ARCH)/Makefile

SRCS	+= printl.c spin.c time.c data_copy.c procctl.c mmap.c get_info.c \
			vmctl.c privctl.c umap.c env.c panic.c irqctl.c fork.c clear.c exec.c signal.c \
			map_phys.c read_tsc_64.c get_procep.c serv_init.c kill.c ksig.c \
			pci_set_acl.c pci_get_dev.c pci_attr.c get_ticks.c trace.c timer.c \
			alarm.c send_async.c asyncsend.c mm_getinfo.c pm_getinfo.c kprofile.c \
			get_epinfo.c idr.c mapdriver.c setgrant.c mgrant.c safecopy.c socketpath.c \
			iov_grant_iter.c assert.c

LIB		= lyos

include lyos.lib.mk
