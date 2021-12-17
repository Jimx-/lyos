#ifndef _ARCH_ASM_H_
#define _ARCH_ASM_H_

#define __ASM_FORM(x, ...) " " #x " "

#ifndef __x86_64__
#define __ASM_SEL(a, b) __ASM_FORM(a)
#else
#define __ASM_SEL(a, b) __ASM_FORM(b)
#endif

#define __ASM_SIZE(inst, ...) __ASM_SEL(inst##l, inst##q)

#endif
