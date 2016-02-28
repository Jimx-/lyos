
SRCS 		= main.c parse.c
EXTRAOBJS 	= cJSON/libcjson.a
EXTRALIBS	= -lm
PROG 		= service

CFLAGS 		+= -I./cJSON

include lyos.prog.mk

cJSON/libcjson.a: cJSON/cJSON.c cJSON/cJSON.h
	$(Q)$(MAKE) -C cJSON $(MAKEFLAGS) libcjson.a