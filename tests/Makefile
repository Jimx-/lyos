SUBDIRS = posix_tests evdev_test pthread_test drm_test inet_test

ifeq ($(CONFIG_VMM),y)
SUBDIRS += vmx
endif

include lyos.subdirs.mk
