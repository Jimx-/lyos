#ifndef _CPULOCALS_H_
#define _CPULOCALS_H_

#ifdef CONFIG_SMP

#ifndef __cpulocals_offset

extern vir_bytes __cpulocals_offset[CONFIG_SMP_MAX_CPUS];
#define cpulocals_offset(cpu) __cpulocals_offset[cpu]

#endif /* __cpulocals_offset */

#define CPULOCAL_BASE_SECTION ".data.cpulocals"

#define get_cpu_var_ptr(cpu, name)                      \
    ({                                                  \
        unsigned long __ptr;                            \
        __ptr = (unsigned long)(&(name));               \
        (typeof(name)*)(__ptr + cpulocals_offset(cpu)); \
    })

#else /* CONFIG_SMP */

#define CPULOCAL_BASE_SECTION ".data"

#define get_cpu_var_ptr(cpu, name) (&(name))

#endif /* CONFIG_SMP */

#define get_cpu_var(cpu, name) (*get_cpu_var_ptr(cpu, name))
#define get_cpulocal_var_ptr(name) get_cpu_var_ptr(cpuid, name)
#define get_cpulocal_var(name) get_cpu_var(cpuid, name)

#define DECLARE_CPULOCAL(type, name) \
    extern __attribute__((section(CPULOCAL_BASE_SECTION))) __typeof__(type) name

#define DEFINE_CPULOCAL(type, name) \
    __attribute__((section(CPULOCAL_BASE_SECTION))) __typeof__(type) name

DECLARE_CPULOCAL(unsigned int, cpu_number);

#endif /* _CPULOCALS_H_ */
