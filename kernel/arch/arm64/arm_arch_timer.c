#include <lyos/types.h>
#include <lyos/ipc.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <asm/proto.h>
#include <kernel/proto.h>
#include <kernel/irq.h>
#include <asm/io.h>
#include <asm/sysreg.h>
#include <string.h>
#include <asm/smp.h>
#include <asm/cpulocals.h>
#include <kernel/clockevent.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include <asm/arm_arch_timer.h>

static u32 arch_timer_rate;

static int arch_timer_ppi[ARCH_TIMER_MAX_TIMER_PPI];
static int arch_timer_uses_ppi;

static irq_hook_t arch_timer_irq_hook;

static DEFINE_CPULOCAL(struct clock_event_device, arch_timer_evt);

static inline u64 arch_timer_reg_read_cp15(int access, enum arch_timer_reg reg)
{
    if (access == ARCH_TIMER_PHYS_ACCESS) {
        switch (reg) {
        case ARCH_TIMER_REG_CTRL:
            return read_sysreg(cntp_ctl_el0);
        default:
            break;
        }
    }

    panic("invalid cp15 read reg");
    return 0;
}

static inline void arch_timer_reg_write_cp15(int access,
                                             enum arch_timer_reg reg, u64 val)
{
    if (access == ARCH_TIMER_PHYS_ACCESS) {
        switch (reg) {
        case ARCH_TIMER_REG_CTRL:
            write_sysreg(val, cntp_ctl_el0);
            isb();
            break;
        case ARCH_TIMER_REG_CVAL:
            write_sysreg(val, cntp_cval_el0);
            break;
        }
    }
}

static inline u32 arch_timer_reg_read(struct clock_event_device* evt,
                                      int access, enum arch_timer_reg reg)
{
    return arch_timer_reg_read_cp15(access, reg);
}

static inline void arch_timer_reg_write(struct clock_event_device* evt,
                                        int access, enum arch_timer_reg reg,
                                        u64 val)
{
    arch_timer_reg_write_cp15(access, reg, val);
}

static int arch_timer_handler(struct clock_event_device* evt, int access)
{
    unsigned long ctrl;

    ctrl = arch_timer_reg_read(evt, access, ARCH_TIMER_REG_CTRL);
    if (ctrl & ARCH_TIMER_CTRL_IT_STAT) {
        ctrl |= ARCH_TIMER_CTRL_IT_MASK;
        arch_timer_reg_write(evt, access, ARCH_TIMER_REG_CTRL, ctrl);
        evt->event_handler(evt);
    }

    return 1;
}

static int arch_timer_handler_phys(struct irq_hook* hook)
{
    struct clock_event_device* evt = get_cpulocal_var_ptr(arch_timer_evt);
    return arch_timer_handler(evt, ARCH_TIMER_PHYS_ACCESS);
}

static inline int timer_shutdown(struct clock_event_device* evt, int access)
{
    unsigned long ctrl;

    ctrl = arch_timer_reg_read(evt, access, ARCH_TIMER_REG_CTRL);
    ctrl &= ~ARCH_TIMER_CTRL_ENABLE;
    arch_timer_reg_write(evt, access, ARCH_TIMER_REG_CTRL, ctrl);

    return 0;
}

static int arch_timer_shutdown_phys(struct clock_event_device* evt)
{
    return timer_shutdown(evt, ARCH_TIMER_PHYS_ACCESS);
}

static inline int arch_timer_set_next_event(struct clock_event_device* evt,
                                            unsigned long delta, int access)
{
    unsigned long ctrl;
    u64 cnt;

    ctrl = arch_timer_reg_read(evt, access, ARCH_TIMER_REG_CTRL);
    ctrl |= ARCH_TIMER_CTRL_ENABLE;
    ctrl &= ~ARCH_TIMER_CTRL_IT_MASK;

    isb();
    if (access == ARCH_TIMER_PHYS_ACCESS)
        cnt = read_sysreg(cntpct_el0);
    else
        cnt = read_sysreg(cntvct_el0);

    arch_timer_reg_write(evt, access, ARCH_TIMER_REG_CVAL, delta + cnt);
    arch_timer_reg_write(evt, access, ARCH_TIMER_REG_CTRL, ctrl);

    return 0;
}

static int arch_timer_set_next_event_phys(struct clock_event_device* evt,
                                          unsigned long delta)
{
    return arch_timer_set_next_event(evt, delta, ARCH_TIMER_PHYS_ACCESS);
}

int arm_arch_timer_setup_cpu(unsigned int cpu)
{
    struct clock_event_device* evt = get_cpulocal_var_ptr(arch_timer_evt);

    evt->rating = 450;
    evt->cpumask = cpumask_of(cpu);
    evt->set_state_shutdown = arch_timer_shutdown_phys;
    evt->set_state_oneshot_stopped = arch_timer_shutdown_phys;
    evt->set_next_event = arch_timer_set_next_event_phys;

    evt->set_state_shutdown(evt);

    clockevents_config_and_register(evt, arch_timer_rate, 0xf,
                                    0x7fffffffffffffffUL);

    enable_irq(&arch_timer_irq_hook);

    return 0;
}

static int fdt_scan_arch_timer(void* blob, unsigned long offset,
                               const char* name, int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "compatible", NULL);
    struct of_phandle_args oirq;
    int i, ret;

    if (!type || (strcmp(type, "arm,armv7-timer") != 0 &&
                  strcmp(type, "arm,armv8-timer") != 0))
        return 0;

    for (i = ARCH_TIMER_PHYS_SECURE_PPI; i < ARCH_TIMER_MAX_TIMER_PPI; i++) {
        unsigned int irq = 0;

        ret = of_irq_parse_one(blob, offset, i, &oirq);
        if (ret == 0) {
            irq = irq_create_of_mapping(&oirq);
        }

        if (irq > 0) arch_timer_ppi[i] = irq;
    }

    arch_timer_rate = read_sysreg(cntfrq_el0);

    arch_timer_uses_ppi = ARCH_TIMER_PHYS_NONSECURE_PPI;

    if (!arch_timer_ppi[arch_timer_uses_ppi]) return 0;

    put_irq_handler(arch_timer_ppi[arch_timer_uses_ppi], &arch_timer_irq_hook,
                    arch_timer_handler_phys);

    return 1;
}

void arm_arch_timer_scan(void)
{
    of_scan_fdt(fdt_scan_arch_timer, NULL, initial_boot_params);
}
