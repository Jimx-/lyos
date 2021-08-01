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
#include "lyos/proc.h"
#include "string.h"
#include <errno.h>
#include <signal.h>
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/csr.h>
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>
#include <asm/io.h>

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
    int nr_irqs;
};

struct plic plics[MAX_PLICS];
unsigned int nr_plics;

struct plic_context {
    void* hart_base;
    void* enable_base;

    struct plic* plic;
};

DEFINE_CPULOCAL(struct plic_context, plic_contexts);

static inline void plic_toggle(struct plic_context* ctx, int hwirq, int enable)
{
    uint32_t* reg = ctx->enable_base + (hwirq / 32) * sizeof(uint32_t);
    uint32_t hwirq_mask = 1 << (hwirq % 32);

    if (enable)
        writel(reg, readl(reg) | hwirq_mask);
    else
        writel(reg, readl(reg) & ~hwirq_mask);
}

static void toggle_irq(struct plic_context* ctx, int hwirq, int enable)
{
    uint32_t* reg = ctx->plic->regs + PRIORITY_BASE + hwirq * PRIORITY_PER_ID;
    writel(reg, enable);

    plic_toggle(ctx, hwirq, enable);
}

void plic_mask(int hwirq)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    toggle_irq(ctx, hwirq, 0);
}

void plic_unmask(int hwirq)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    toggle_irq(ctx, hwirq, 1);
}

void plic_ack(int hwirq)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    writel(ctx->hart_base + CONTEXT_CLAIM, hwirq);
}

static int fdt_scan_plic(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct plic* plic;
    const char* type = fdt_getprop(blob, offset, "compatible", NULL);

    if (!type || strcmp(type, "riscv,plic0") != 0) return 0;

    plic = &plics[nr_plics++];

    const uint32_t* reg;
    int len;
    reg = fdt_getprop(blob, offset, "reg", &len);
    if (!reg) return 0;

    unsigned long base, size;
    base = of_read_number(reg, dt_root_addr_cells);
    reg += dt_root_addr_cells;
    size = of_read_number(reg, dt_root_size_cells);

    kern_map_phys(base, size, KMF_WRITE, &plic->regs);

    const uint32_t* ndev;
    ndev = fdt_getprop(blob, offset, "riscv,ndev", &len);
    if (!ndev) return 0;

    plic->nr_irqs = of_read_number(ndev, 1);

    return 0;
}

void plic_scan(void) { of_scan_fdt(fdt_scan_plic, NULL, initial_boot_params); }

void plic_init(void)
{
    int i;

    for (i = 0; i < nr_plics; i++) {
        struct plic* plic = &plics[i];
        int cpu = 0;

        struct plic_context* ctx = get_cpu_var_ptr(cpu, plic_contexts);
        ctx->plic = plic;
        ctx->hart_base = plic->regs + CONTEXT_BASE + 0 * CONTEXT_PER_HART;
        ctx->enable_base = plic->regs + ENABLE_BASE + 0 * ENABLE_PER_HART;

        int hwirq;
        for (hwirq = 1; hwirq <= plic->nr_irqs; hwirq++)
            plic_toggle(ctx, hwirq, 0);
    }
}

static void plic_set_threshold(struct plic_context* ctx, uint32_t threshold)
{
    uint32_t* reg = ctx->hart_base + CONTEXT_THRESHOLD;
    writel(reg, threshold);
}

void plic_enable(int cpu)
{
    struct plic_context* ctx = get_cpu_var_ptr(cpu, plic_contexts);
    csr_set(sie, SIE_SEIE);
    plic_set_threshold(ctx, PLIC_ENABLE_THRESHOLD);
}

void plic_handle_irq(void)
{
    struct plic_context* ctx = get_cpulocal_var_ptr(plic_contexts);
    void* claim = ctx->hart_base + CONTEXT_CLAIM;
    int hwirq;

    csr_clear(sie, SIE_SEIE);
    while ((hwirq = readl(claim))) {
        irq_handle(hwirq);
    }

    csr_set(sie, SIE_SEIE);
}
