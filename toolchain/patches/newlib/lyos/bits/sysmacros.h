#ifndef _BITS_SYSMACROS_H_
#define _BITS_SYSMACROS_H_

#ifndef _SYS_SYSMACROS_H_
#error \
    "Never include <bits/sysmacros.h> directly; use <sys/sysmacros.h> instead."
#endif

#define makedev(a, b) (((a) << 8) | (b))
#define major(x)      ((dev_t)(x >> 8))
#define minor(x)      ((dev_t)(x & 0xff))

#endif
