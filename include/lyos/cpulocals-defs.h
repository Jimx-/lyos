#ifndef _CPULOCALS_DEFS_H_
#define _CPULOCALS_DEFS_H_

#ifdef CONFIG_SMP
#define CPULOCAL_BASE_SECTION ".data.cpulocals"
#else
#define CPULOCAL_BASE_SECTION ".data"
#endif

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
