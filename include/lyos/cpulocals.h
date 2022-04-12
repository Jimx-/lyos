#ifndef _CPULOCALS_H_
#define _CPULOCALS_H_

#ifdef __aarch64__
#include <asm/cpulocals.h>
#endif

#ifdef CONFIG_SMP

#ifndef __cpulocals_offset

extern vir_bytes __cpulocals_offset[CONFIG_SMP_MAX_CPUS];
#define cpulocals_offset(cpu) __cpulocals_offset[cpu]

#endif /* __cpulocals_offset */

#ifndef __my_cpu_offset
#define __my_cpu_offset (cpulocals_offset(cpuid))
#endif

#define CPULOCAL_BASE_SECTION ".data.cpulocals"

#define __get_cpu_var_ptr_offset(offset, name) \
    ({                                         \
        unsigned long __ptr;                   \
        __ptr = (unsigned long)(&(name));      \
        (typeof(name)*)(__ptr + offset);       \
    })

#else /* CONFIG_SMP */

#define CPULOCAL_BASE_SECTION ".data"

#define get_cpu_var_ptr_offset(offset, name) (&(name))

#endif /* CONFIG_SMP */

#define get_cpu_var_ptr(cpu, name) \
    __get_cpu_var_ptr_offset(cpulocals_offset(cpu), name)
#define get_cpu_var(cpu, name) (*get_cpu_var_ptr(cpu, name))
#define get_cpulocal_var_ptr(name) \
    __get_cpu_var_ptr_offset(__my_cpu_offset, name)
#define get_cpulocal_var(name) (*get_cpulocal_var_ptr(name))

#define DECLARE_CPULOCAL(type, name) \
    extern __attribute__((section(CPULOCAL_BASE_SECTION))) __typeof__(type) name

#define DEFINE_CPULOCAL(type, name) \
    __attribute__((section(CPULOCAL_BASE_SECTION))) __typeof__(type) name

#endif /* _CPULOCALS_H_ */
