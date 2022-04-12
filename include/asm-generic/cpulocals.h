#ifndef _ASM_GENERIC_CPULOCALS_H_
#define _ASM_GENERIC_CPULOCALS_H_

#include <lyos/cpulocals-defs.h>

#ifdef CONFIG_SMP

#ifndef __cpulocals_offset

extern vir_bytes __cpulocals_offset[CONFIG_SMP_MAX_CPUS];
#define cpulocals_offset(cpu) __cpulocals_offset[cpu]

#endif /* __cpulocals_offset */

#ifndef __my_cpu_offset
#define __my_cpu_offset (cpulocals_offset(cpuid))
#endif

#define __get_cpu_var_ptr_offset(offset, name) \
    ({                                         \
        unsigned long __ptr;                   \
        __ptr = (unsigned long)(&(name));      \
        (typeof(name)*)(__ptr + offset);       \
    })

#else /* CONFIG_SMP */

#define get_cpu_var_ptr_offset(offset, name) (&(name))

#endif /* CONFIG_SMP */

#endif
