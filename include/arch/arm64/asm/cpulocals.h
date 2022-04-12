#ifndef _ARCH_CPULOCALS_H_
#define _ARCH_CPULOCALS_H_

static inline unsigned long ___my_cpu_offset(void)
{
    unsigned long off;
    register unsigned long current_sp asm("sp");

    /*
     * We want to allow caching the value, so avoid using volatile and
     * instead use a fake stack read to hazard against barrier().
     */
    asm("mrs %0, tpidr_el1"
        : "=r"(off)
        : "Q"(*(const unsigned long*)current_sp));

    return off;
}

#define __my_cpu_offset ___my_cpu_offset()

#include <asm-generic/cpulocals.h>

#endif
