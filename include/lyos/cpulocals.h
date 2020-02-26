#ifndef _CPULOCALS_H_
#define _CPULOCALS_H_

#define CPULOCAL_STRUCT __cpu_local_vars

#ifdef CONFIG_SMP

#define get_cpu_var(cpu, name) CPULOCAL_STRUCT[cpu].name
#define get_cpu_var_ptr(cpu, name) (&(get_cpu_var(cpu, name)))
#define get_cpulocal_var(name) get_cpu_var(cpuid, name)
#define get_cpulocal_var_ptr(name) get_cpu_var_ptr(cpuid, name)

#else

#define get_cpulocal_var(name) CPULOCAL_STRUCT.name
#define get_cpulocal_var_ptr(name) &(get_cpulocal_var(name))
#define get_cpu_var(cpu, name) get_cpulocal_var(name)
#define get_cpu_var_ptr(cpu, name) get_cpulocal_var_ptr(name)

#endif

extern struct CPULOCAL_STRUCT {
    struct proc* proc_ptr;
    struct proc* pt_proc; /* proc whose page table is loaded */
    struct proc idle_proc;

    /* run queue */
    struct proc* run_queue_head[NR_SCHED_QUEUES];
    struct proc* run_queue_tail[NR_SCHED_QUEUES];

    volatile int cpu_is_idle;

    int fpu_present;

    u64 context_switch_clock;

#ifdef CONFIG_SMP
} CPULOCAL_STRUCT[CONFIG_SMP_MAX_CPUS];
#else
} CPULOCAL_STRUCT;
#endif

#endif /* _CPULOCALS_H_ */
