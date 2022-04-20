/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include <kernel/proc.h>
#include "string.h"
#include <errno.h>
#include <signal.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/csr.h>
#include <asm/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>
#include <asm/io.h>
#include <kernel/irq.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#define PRIORITY_BASE   0
#define PRIORITY_PER_ID 4

#define ENABLE_BASE     0x2000
#define ENABLE_PER_HART 0x80

#define CONTEXT_BASE      0x200000
#define CONTEXT_PER_HART  0x1000
#define CONTEXT_THRESHOLD 0x00
#define CONTEXT_CLAIM     0x04

#define PLIC_DISABLE_THRESHOLD 0x7
#define PLIC_ENABLE_THRESHOLD  0

#define MAX_PLICS 8

struct plic {
    void* regs;
    u32 phandle;
    int nr_irqs;
    struct irq_domain* irqdomain;
    unsigned long fdt_offset;
};

struct plic plics[MAX_PLICS];
unsigned int nr_plics;

struct plic_context {
    void* hart_base;
    void* enable_base;

    struct plic* plic;
};

DEFINE_CPULOCAL(struct plic_context, plic_contexts);

static const struct irq_domain_ops plic_irqdomain_ops;

static inline void plic_toggle(struct plic_context* ctx, int hwirq, int enable)
{
    u32* reg = ctx->enable_base + (hwirq / 32) * sizeof(u32);
    u32 hwirq_mask = 1 << (hwirq % 32);

    if (enable)
        writel(reg, readl(reg) | hwirq_mask);
    else
        writel(reg, readl(reg) & ~hwirq_mask);
}

static void toggle_irq(struct plic_context* ctx, int hwirq, int enable)
{
    u32* reg = ctx->plic->regs + PRIORITY_BASE + hwirq * PRIORITY_PER_ID;
    writel(reg, enable);

    plic_toggle(ctx, hwirq, enable);
}

static void plic_mask(struct irq_data* data)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    toggle_irq(ctx, data->hwirq, 0);
}

static void plic_unmask(struct irq_data* data)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    toggle_irq(ctx, data->hwirq, 1);
}

static void plic_ack(struct irq_data* data)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    writel(ctx->hart_base + CONTEXT_CLAIM, data->hwirq);
}

static struct irq_chip plic_chip = {
    .irq_mask = plic_mask,
    .irq_unmask = plic_unmask,
    .irq_eoi = plic_ack,
};

void plic_init(void* arg)
{
    int i, j;
    struct plic* plic = (struct plic*)arg;
    int cpu, nr_contexts;

    nr_contexts = of_irq_count(initial_boot_params, plic->fdt_offset);

    plic->irqdomain = irq_domain_add(plic->phandle, plic->nr_irqs + 1,
                                     &plic_irqdomain_ops, plic);
    if (!plic->irqdomain)
        panic("failed to allocate IRQ domain for PLIC %d\n", plic->phandle);

    for (j = 0; j < nr_contexts; j++) {
        struct of_phandle_args parent;
        u32 hart;

        if (of_irq_parse_one(initial_boot_params, plic->fdt_offset, j, &parent))
            continue;

        if (parent.args[0] != IRQ_S_EXT) continue;

        hart = riscv_of_parent_hartid(initial_boot_params, parent.offset);
        cpu = hart_to_cpu_id[hart];

        struct plic_context* ctx = get_cpu_var_ptr(cpu, plic_contexts);
        ctx->plic = plic;
        ctx->hart_base = plic->regs + CONTEXT_BASE + j * CONTEXT_PER_HART;
        ctx->enable_base = plic->regs + ENABLE_BASE + j * ENABLE_PER_HART;

        int hwirq;
        for (hwirq = 1; hwirq <= plic->nr_irqs; hwirq++)
            plic_toggle(ctx, hwirq, 0);
    }
}

static int fdt_scan_plic(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct plic* plic;
    const char* type = fdt_getprop(blob, offset, "compatible", NULL);
    unsigned long base, size;
    const u32 *reg, *prop;
    int len;

    if (!type || (strcmp(type, "sifive,plic-1.0.0") != 0 &&
                  strcmp(type, "riscv,plic0") != 0))
        return 0;

    plic = &plics[nr_plics];

    reg = fdt_getprop(blob, offset, "reg", NULL);
    if (!reg) return 0;

    base = of_read_number(reg, dt_root_addr_cells);
    reg += dt_root_addr_cells;
    size = of_read_number(reg, dt_root_size_cells);

    prop = fdt_getprop(blob, offset, "phandle", NULL);
    plic->phandle = of_read_number(prop, 1);

    prop = fdt_getprop(blob, offset, "riscv,ndev", NULL);
    if (!prop) return 0;
    plic->nr_irqs = of_read_number(prop, 1);

    plic->fdt_offset = offset;
    kern_map_phys(base, size, KMF_WRITE, &plic->regs, plic_init, plic);
    nr_plics++;

    return nr_plics >= MAX_PLICS;
}

void plic_scan(void) { of_scan_fdt(fdt_scan_plic, NULL, initial_boot_params); }

static void plic_set_threshold(struct plic_context* ctx, u32 threshold)
{
    u32* reg = ctx->hart_base + CONTEXT_THRESHOLD;
    writel(reg, threshold);
}

void plic_enable(int cpu)
{
    struct plic_context* ctx = get_cpu_var_ptr(cpu, plic_contexts);
    csr_set(sie, SIE_SEIE);
    plic_set_threshold(ctx, PLIC_ENABLE_THRESHOLD);
}

static int plic_irq_domain_alloc(struct irq_domain* domain, unsigned int virq,
                                 unsigned int nr_irqs, void* arg)
{
    struct irq_fwspec* fwspec = arg;
    unsigned int hwirq;
    unsigned int type;
    int i, ret;

    ret = irq_domain_translate_onecell(domain, fwspec, &hwirq, &type);
    if (ret) return ret;

    for (i = 0; i < nr_irqs; i++) {
        ret = irq_domain_set_hwirq_and_chip(domain, virq + i, hwirq + i,
                                            &plic_chip, domain->host_data);
        if (ret) return ret;
    }

    return 0;
}

static const struct irq_domain_ops plic_irqdomain_ops = {
    .alloc = plic_irq_domain_alloc,
    .translate = irq_domain_translate_onecell,
};

void plic_handle_irq(void)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    void* claim = ctx->hart_base + CONTEXT_CLAIM;
    int hwirq;

    csr_clear(sie, SIE_SEIE);
    while ((hwirq = readl(claim))) {
        handle_domain_irq(ctx->plic->irqdomain, hwirq);
    }

    csr_set(sie, SIE_SEIE);
}
