#include <lyos/types.h>
#include <lyos/ipc.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/irq.h>
#include <lyos/vm.h>
#include <asm/proto.h>
#include <kernel/proto.h>
#include <asm/io.h>
#include <asm/smp.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "bcm2836-irq.h"

#define NAME "bcm2836_arm_irqchip_intc"

struct bcm2836_arm_irqchip_intc {
    struct irq_domain* domain;
    void* base;
    u32 phandle;
    unsigned long fdt_offset;
};

static struct bcm2836_arm_irqchip_intc intc;

static void bcm2836_arm_irqchip_mask_per_cpu_irq(unsigned int reg_offset,
                                                 unsigned int bit, int cpu)
{
    void* reg;

    if (!intc.base) return;

    reg = intc.base + reg_offset + 4 * cpu;
    writel(reg, readl(reg) & ~BIT(bit));
}

static void bcm2836_arm_irqchip_unmask_per_cpu_irq(unsigned int reg_offset,
                                                   unsigned int bit, int cpu)
{
    void* reg;

    if (!intc.base) return;

    reg = intc.base + reg_offset + 4 * cpu;
    writel(reg, readl(reg) | BIT(bit));
}

static void bcm2836_arm_irqchip_mask_timer_irq(struct irq_data* d)
{
    bcm2836_arm_irqchip_mask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0,
                                         d->hwirq - LOCAL_IRQ_CNTPSIRQ, cpuid);
}

static void bcm2836_arm_irqchip_unmask_timer_irq(struct irq_data* d)
{
    bcm2836_arm_irqchip_unmask_per_cpu_irq(
        LOCAL_TIMER_INT_CONTROL0, d->hwirq - LOCAL_IRQ_CNTPSIRQ, cpuid);
}

static struct irq_chip bcm2836_arm_irqchip_timer = {
    .irq_mask = bcm2836_arm_irqchip_mask_timer_irq,
    .irq_unmask = bcm2836_arm_irqchip_unmask_timer_irq,
};

static int bcm2836_map(struct irq_domain* d, unsigned int irq, unsigned int hw)
{
    struct irq_chip* chip;

    switch (hw) {
    case LOCAL_IRQ_CNTPSIRQ:
    case LOCAL_IRQ_CNTPNSIRQ:
    case LOCAL_IRQ_CNTHPIRQ:
    case LOCAL_IRQ_CNTVIRQ:
        chip = &bcm2836_arm_irqchip_timer;
        break;
    }

    irq_domain_set_info(d, irq, hw, chip, d->host_data, handle_simple_irq);

    return 0;
}

static int bcm2836_irq_domain_alloc(struct irq_domain* domain,
                                    unsigned int virq, unsigned int nr_irqs,
                                    void* arg)
{
    struct irq_fwspec* fwspec = arg;
    unsigned int hwirq;
    unsigned int type;
    int i, ret;

    ret = irq_domain_translate_onetwocell(domain, fwspec, &hwirq, &type);
    if (ret) return ret;

    for (i = 0; i < nr_irqs; i++) {
        ret = bcm2836_map(domain, virq + i, hwirq + i);
        if (ret) return ret;
    }

    return 0;
}

static const struct irq_domain_ops bcm2836_arm_irqchip_intc_ops = {
    .translate = irq_domain_translate_onetwocell,
    .alloc = bcm2836_irq_domain_alloc,
};

static void bcm2835_init_local_timer_frequency(void)
{
    writel(intc.base + LOCAL_CONTROL, 0);
    writel(intc.base + LOCAL_PRESCALER, 0x80000000);
}

static void bcm2836_arm_irqchip_l1_intc_init(void* arg)
{
    bcm2835_init_local_timer_frequency();
}

static int fdt_scan_local_intc(void* blob, unsigned long offset,
                               const char* name, int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "compatible", NULL);
    phys_bytes base, size;
    const __be32* prop;
    int ret;

    if (!type || strcmp(type, "brcm,bcm2836-l1-intc") != 0) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    prop = fdt_getprop(blob, offset, "phandle", NULL);
    intc.phandle = of_read_number(prop, 1);

    intc.fdt_offset = offset;
    kern_map_phys(base, size, KMF_WRITE | KMF_IO, &intc.base,
                  bcm2836_arm_irqchip_l1_intc_init, NULL);

    intc.domain = irq_domain_add(intc.phandle, LAST_IRQ + 1,
                                 &bcm2836_arm_irqchip_intc_ops, NULL);
    if (!intc.domain) panic("%s: unable to create IRQ domain", NAME);

    return 1;
}

void bcm2836_arm_irqchip_l1_intc_scan(void)
{
    of_scan_fdt(fdt_scan_local_intc, NULL, initial_boot_params);
}

void bcm2836_arm_irqchip_handle_irq(void)
{
    int cpu = cpuid;
    u32 stat;

    stat = readl(intc.base + LOCAL_IRQ_PENDING0 + 4 * cpu);
    if (stat) {
        u32 hwirq = __builtin_ffs(stat) - 1;

        generic_handle_domain_irq(intc.domain, hwirq);
    }
}
