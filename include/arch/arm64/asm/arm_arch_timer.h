#ifndef _ARM_ARCH_TIMER_H_
#define _ARM_ARCH_TIMER_H_

#include <lyos/const.h>

#define ARCH_TIMER_CTRL_ENABLE  (1 << 0)
#define ARCH_TIMER_CTRL_IT_MASK (1 << 1)
#define ARCH_TIMER_CTRL_IT_STAT (1 << 2)

enum arch_timer_ppi_nr {
    ARCH_TIMER_PHYS_SECURE_PPI,
    ARCH_TIMER_PHYS_NONSECURE_PPI,
    ARCH_TIMER_VIRT_PPI,
    ARCH_TIMER_HYP_PPI,
    ARCH_TIMER_HYP_VIRT_PPI,
    ARCH_TIMER_MAX_TIMER_PPI
};

enum arch_timer_reg {
    ARCH_TIMER_REG_CTRL,
    ARCH_TIMER_REG_CVAL,
};

#define ARCH_TIMER_PHYS_ACCESS 0
#define ARCH_TIMER_VIRT_ACCESS 1

void arm_arch_timer_scan(void);
int arm_arch_timer_setup_cpu(unsigned int cpu);

#endif
