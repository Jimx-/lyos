#ifndef _ARCH_STRING_H_
#define _ARCH_STRING_H_

#include <lyos/config.h>

#ifdef CONFIG_X86_32
#else
#include <asm/string_64.h>
#endif

#endif
