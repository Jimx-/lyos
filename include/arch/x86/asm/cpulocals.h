#ifndef _ARCH_CPULOCALS_H_
#define _ARCH_CPULOCALS_H_

#ifdef CONFIG_X86_32
#define __cpulocals_seg "fs"
#else
#define __cpulocals_seg "gs"
#endif

#ifdef CONFIG_SMP

#define __cpulocals_seg_prefix "%%" __cpulocals_seg ":"

#else
#define __cpulocals_seg_prefix ""
#endif

#define __cpulocal_arg(x) __cpulocals_seg_prefix "%" #x

#define cpulocal_from_op(qual, op, var)              \
    ({                                               \
        typeof(var) __cfo_ret;                       \
        switch (sizeof(var)) {                       \
        case 4:                                      \
            asm qual(op "l " __cpulocal_arg(1) ",%0" \
                     : "=r"(__cfo_ret)               \
                     : "m"(var));                    \
            break;                                   \
        }                                            \
        __cfo_ret;                                   \
    })

#define raw_cpulocal_read(var) cpulocal_from_op(, "mov", var)

#endif
