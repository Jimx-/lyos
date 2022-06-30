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
#include <errno.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#define NAME "bcm2835-armctrl"

#define MAKE_HWIRQ(b, n) ((b << 5) | (n))
#define HWIRQ_BANK(i)    (i >> 5)
#define HWIRQ_BIT(i)     BIT(i & 0x1f)

#define NR_BANKS      3
#define IRQS_PER_BANK 32

#define REG_FIQ_CONTROL    0x0c
#define FIQ_CONTROL_ENABLE BIT(7)

static const int reg_pending[] = {0x00, 0x04, 0x08};
static const int reg_enable[] = {0x18, 0x10, 0x14};
static const int reg_disable[] = {0x24, 0x1c, 0x20};
static const int bank_irqs[] = {8, 32, 32};

#define BANK0_HWIRQ_MASK 0xff
#define SHORTCUT1_MASK   0x00007c00
#define SHORTCUT2_MASK   0x001f8000
#define SHORTCUT_SHIFT   10
#define BANK1_HWIRQ      BIT(8)
#define BANK2_HWIRQ      BIT(9)
#define BANK0_VALID_MASK                                             \
    (BANK0_HWIRQ_MASK | BANK1_HWIRQ | BANK2_HWIRQ | SHORTCUT1_MASK | \
     SHORTCUT2_MASK)

static const int shortcuts[] = {7, 9, 10, 18, 19, 21, 22, 23, 24, 25, 30};

struct armctrl_ic {
    void* base;
    void* pending[NR_BANKS];
    void* enable[NR_BANKS];
    void* disable[NR_BANKS];
    struct irq_domain* domain;

    u32 phandle;
    unsigned long fdt_offset;
    int is_2836;
};

static struct armctrl_ic intc;

static void bcm2836_chained_handle_irq(struct irq_desc* desc);

static void armctrl_mask_irq(struct irq_data* d)
{
    writel(intc.disable[HWIRQ_BANK(d->hwirq)], HWIRQ_BIT(d->hwirq));
}

static void armctrl_unmask_irq(struct irq_data* d)
{
    writel(intc.enable[HWIRQ_BANK(d->hwirq)], HWIRQ_BIT(d->hwirq));
}

static struct irq_chip armctrl_chip = {.irq_mask = armctrl_mask_irq,
                                       .irq_unmask = armctrl_unmask_irq};

int armctrl_translate(struct irq_domain* d, struct irq_fwspec* fwspec,
                      unsigned int* out_hwirq, unsigned int* out_type)
{
    if (fwspec->param_count != 2) return -EINVAL;
    if (fwspec->param[0] >= NR_BANKS) return -EINVAL;
    if (fwspec->param[1] >= IRQS_PER_BANK) return -EINVAL;
    if (fwspec->param[0] == 0 && fwspec->param[1] >= bank_irqs[0])
        return -EINVAL;

    *out_hwirq = MAKE_HWIRQ(fwspec->param[0], fwspec->param[1]);
    *out_type = IRQ_TYPE_NONE;

    return 0;
}

static int armctrl_irq_domain_alloc(struct irq_domain* domain,
                                    unsigned int virq, unsigned int nr_irqs,
                                    void* arg)
{
    struct irq_fwspec* fwspec = arg;
    unsigned int hwirq;
    unsigned int type;
    int i, ret;

    ret = domain->ops->translate(domain, fwspec, &hwirq, &type);
    if (ret) return ret;

    for (i = 0; i < nr_irqs; i++) {
        irq_domain_set_info(domain, virq + i, hwirq + i, &armctrl_chip,
                            domain->host_data, handle_simple_irq);
    }

    return 0;
}

static const struct irq_domain_ops armctrl_ops = {
    .translate = armctrl_translate,
    .alloc = armctrl_irq_domain_alloc,
};

static void bcm2835_armctrl_irqchip_init(void* arg)
{
    int b;
    u32 reg;
    int ret;

    for (b = 0; b < NR_BANKS; b++) {
        intc.pending[b] = intc.base + reg_pending[b];
        intc.enable[b] = intc.base + reg_enable[b];
        intc.disable[b] = intc.base + reg_disable[b];

        reg = readl(intc.enable[b]);
        if (reg) writel(intc.disable[b], reg);
    }

    reg = readl(intc.base + REG_FIQ_CONTROL);
    if (reg & FIQ_CONTROL_ENABLE) writel(intc.base + REG_FIQ_CONTROL, 0);

    if (intc.is_2836) {
        struct of_phandle_args oirq;
        int parent_irq = 0;

        ret = of_irq_parse_one(initial_boot_params, intc.fdt_offset, 0, &oirq);
        if (ret == 0) {
            parent_irq = irq_create_of_mapping(&oirq);
        }

        irq_set_handler(parent_irq, bcm2836_chained_handle_irq, TRUE);
    }
}

static int fdt_scan_armctrl(void* blob, unsigned long offset, const char* name,
                            int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "compatible", NULL);
    phys_bytes base, size;
    const __be32* prop;
    int ret;

    if (!type || (strcmp(type, "brcm,bcm2835-armctrl-ic") != 0 &&
                  strcmp(type, "brcm,bcm2836-armctrl-ic") != 0))
        return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    intc.phandle = fdt_get_phandle(blob, offset);

    intc.fdt_offset = offset;
    kern_map_phys(base, size, KMF_WRITE | KMF_IO, &intc.base,
                  bcm2835_armctrl_irqchip_init, NULL);

    intc.domain = irq_domain_add(intc.phandle, MAKE_HWIRQ(NR_BANKS, 0),
                                 &armctrl_ops, NULL);
    if (!intc.domain) panic("%s: unable to create IRQ domain", NAME);

    intc.is_2836 = strcmp(type, "brcm,bcm2836-armctrl-ic") == 0;

    return 1;
}

void bcm2835_armctrl_irqchip_scan(void)
{
    of_scan_fdt(fdt_scan_armctrl, NULL, initial_boot_params);
}

static u32 armctrl_translate_bank(int bank)
{
    u32 stat = readl(intc.pending[bank]);

    return MAKE_HWIRQ(bank, __builtin_ffs(stat) - 1);
}

static u32 armctrl_translate_shortcut(int bank, u32 stat)
{
    return MAKE_HWIRQ(bank,
                      shortcuts[__builtin_ffs(stat >> SHORTCUT_SHIFT) - 1]);
}

static u32 get_next_armctrl_hwirq(void)
{
    u32 stat = readl(intc.pending[0]) & BANK0_VALID_MASK;

    if (stat == 0)
        return ~0;
    else if (stat & BANK0_HWIRQ_MASK)
        return MAKE_HWIRQ(0, __builtin_ffs(stat & BANK0_HWIRQ_MASK) - 1);
    else if (stat & SHORTCUT1_MASK)
        return armctrl_translate_shortcut(1, stat & SHORTCUT1_MASK);
    else if (stat & SHORTCUT2_MASK)
        return armctrl_translate_shortcut(2, stat & SHORTCUT2_MASK);
    else if (stat & BANK1_HWIRQ)
        return armctrl_translate_bank(1);
    else if (stat & BANK2_HWIRQ)
        return armctrl_translate_bank(2);

    return ~0;
}

static void bcm2836_chained_handle_irq(struct irq_desc* desc)
{
    u32 hwirq;

    while ((hwirq = get_next_armctrl_hwirq()) != ~0)
        generic_handle_domain_irq(intc.domain, hwirq);
}
