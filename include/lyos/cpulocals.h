#ifndef _CPULOCALS_H_
#define _CPULOCALS_H_

#define CPULOCAL_STRUCT			__cpu_local_vars

#ifdef CONFIG_SMP

#define get_cpu_var(cpu, name)		CPULOCAL_STRUCT[cpu].name
#define get_cpu_var_ptr(cpu, name)	(&(get_cpu_var(cpu, name)))
#define get_cpulocal_var(name)		get_cpu_var(cpuid, name)
#define get_cpulocal_var_ptr(name)	get_cpu_var_ptr(cpuid, name)

#else

#define get_cpulocal_var(name)		CPULOCAL_STRUCT.name
#define get_cpulocal_var_ptr(name)	&(get_cpulocal_var(name))
#define get_cpu_var(cpu, name)		get_cpulocal_var(name)
#define get_cpu_var_ptr(cpu, name)	get_cpulocal_var_ptr(name)

#endif

extern struct CPULOCAL_STRUCT {
	struct proc * proc_ptr;
	struct proc idle_proc;

	volatile u8 cpu_is_idle;
	
#ifdef CONFIG_SMP
} CPULOCAL_STRUCT[CONFIG_SMP_MAX_CPUS];
#else
} CPULOCAL_STRUCT;
#endif

#endif /* _CPULOCALS_H_ */
