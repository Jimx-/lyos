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

#include <lyos/ipc.h>
#include <lyos/list.h>
#include <errno.h>
#include "lyos/const.h"
#include <kernel/proto.h>
#include <asm/page.h>
#include <asm/proto.h>
#include <kernel/irq.h>
#include <lyos/bitmap.h>
#include <lyos/spinlock.h>
#include <string.h>

#if CONFIG_OF
#include <libof/libof.h>
#endif

static struct irq_desc irq_desc[NR_IRQ];

static DEF_SPINLOCK(irq_domain_lock);
static struct irq_domain irq_domain[NR_IRQ_DOMAIN];

static DEF_SPINLOCK(irq_allocate_lock);
static bitchunk_t allocated_irqs[BITCHUNKS(NR_IRQ)];

static struct irq_chip no_irq_chip = {};

struct irq_desc* irq_resolve_mapping(struct irq_domain* domain,
                                     unsigned int hwirq, unsigned int* irq);

static inline struct irq_desc* irq_to_desc(unsigned int irq)
{
    if (irq >= NR_IRQ) return NULL;
    return &irq_desc[irq];
}

static inline struct irq_data* irq_get_irq_data(unsigned int irq)
{
    struct irq_desc* desc = irq_to_desc(irq);
    return desc ? &desc->irq_data : NULL;
}

static inline struct irq_desc* irq_data_to_desc(struct irq_data* irq_data)
{
    return list_entry(irq_data, struct irq_desc, irq_data);
}

static void desc_set_defaults(unsigned int irq, struct irq_desc* desc)
{
    desc->handle_irq = handle_bad_irq;
    desc->irq_data.irq = irq;
    desc->irq_data.chip = &no_irq_chip;
    desc->irq_data.chip_data = NULL;
}

int irq_set_chip(unsigned int irq, const struct irq_chip* chip)
{
    struct irq_desc* desc = irq_to_desc(irq);

    if (!desc) return EINVAL;
    spinlock_lock(&desc->lock);
    desc->irq_data.chip = (struct irq_chip*)(chip ?: &no_irq_chip);
    spinlock_unlock(&desc->lock);

    return 0;
}

void irq_set_handler(unsigned int irq, irq_flow_handler_t handler,
                     int is_chained)
{
    struct irq_desc* desc = irq_to_desc(irq);

    if (!desc) return;
    spinlock_lock(&desc->lock);

    if (!handler) handler = handle_bad_irq;
    desc->handle_irq = handler;

    spinlock_unlock(&desc->lock);
}

/*****************************************************************************
 *                                init_irq
 *****************************************************************************/
/**
 * <Ring 0> Initializes IRQ subsystem.
 *
 *****************************************************************************/
void early_init_irq(void)
{
    int i;
    for (i = 0; i < NR_IRQ_HOOKS; i++) {
        irq_hooks[i].proc_ep = NO_TASK;
    }

    for (i = 0; i < NR_IRQ; i++) {
        spinlock_init(&irq_desc[i].lock);
        desc_set_defaults(i, &irq_desc[i]);
    }

    for (i = 0; i < NR_IRQ_DOMAIN; i++) {
        spinlock_init(&irq_domain[i].revmap_lock);
        irq_domain->fwid = IRQ_DOMAIN_NONE;
    }
}

void init_irq(void) { arch_init_irq(); }

void mask_irq(struct irq_desc* desc)
{
    if (desc->irq_data.chip->irq_mask)
        desc->irq_data.chip->irq_mask(&desc->irq_data);
}

void unmask_irq(struct irq_desc* desc)
{
    if (desc->irq_data.chip->irq_unmask)
        desc->irq_data.chip->irq_unmask(&desc->irq_data);
}

/*****************************************************************************
 *                                put_irq_handler
 *****************************************************************************/
/**
 * <Ring 0> Register an IRQ handler.
 *
 *****************************************************************************/
void put_irq_handler(int irq, irq_hook_t* hook, irq_handler_t handler)
{
    struct irq_desc* desc = irq_to_desc(irq);

    spinlock_lock(&desc->lock);
    irq_hook_t** line = &desc->handler;

    unsigned long used_ids = 0;
    while (*line != NULL) {
        if (hook == *line) {
            spinlock_unlock(&desc->lock);
            return;
        }
        used_ids |= (*line)->id;
        line = &(*line)->next;
    }

    unsigned long id;
    for (id = 1; id != 0; id <<= 1)
        if ((used_ids & id) == 0) break;

    if (id == 0) panic("too many handlers for irq %d", irq);

    hook->next = NULL;
    hook->handler = handler;
    hook->id = id;
    hook->irq = irq;
    *line = hook;

    if ((desc->active_ids &= ~hook->id) == 0) {
        if (desc->irq_data.chip->irq_used)
            desc->irq_data.chip->irq_used(&desc->irq_data);
        unmask_irq(desc);
    }

    spinlock_unlock(&desc->lock);
}

void rm_irq_handler(irq_hook_t* hook)
{
    int irq = hook->irq;
    unsigned long id = hook->id;
    struct irq_desc* desc = irq_to_desc(irq);

    spinlock_lock(&desc->lock);
    irq_hook_t** line = &desc->handler;
    while (*line != NULL) {
        if ((*line)->id == id) {
            (*line) = (*line)->next;
            if (desc->active_ids & id) desc->active_ids &= ~id;
        } else
            line = &(*line)->next;
    }

    if (desc->handler == NULL) {
        mask_irq(desc);
        if (desc->irq_data.chip->irq_not_used)
            desc->irq_data.chip->irq_not_used(&desc->irq_data);
    } else if (desc->active_ids == 0)
        unmask_irq(desc);

    spinlock_unlock(&desc->lock);
}

void handle_bad_irq(struct irq_desc* desc) {}

void handle_simple_irq(struct irq_desc* desc)
{
    irq_hook_t* hook;

    spinlock_lock(&desc->lock);
    hook = desc->handler;

    mask_irq(desc);

    while (hook != NULL) {
        desc->active_ids |= hook->id;

        if ((*hook->handler)(hook)) /* reenable int */
            desc->active_ids &= ~hook->id;

        hook = hook->next;
    }

    if (desc->active_ids == 0) unmask_irq(desc);

    if (desc->irq_data.chip->irq_eoi) {
        desc->irq_data.chip->irq_eoi(&desc->irq_data);
    }

    spinlock_unlock(&desc->lock);
}

static void handle_irq_desc(struct irq_desc* desc)
{
    if (!desc) return;
    desc->handle_irq(desc);
}

void generic_handle_domain_irq(struct irq_domain* domain, unsigned int hwirq)
{
    handle_irq_desc(irq_resolve_mapping(domain, hwirq, NULL));
}

void generic_handle_irq(unsigned int irq) { handle_irq_desc(irq_to_desc(irq)); }

void enable_irq(irq_hook_t* hook)
{
    struct irq_desc* desc = irq_to_desc(hook->irq);

    spinlock_lock(&desc->lock);

    if ((desc->active_ids &= ~hook->id) == 0) {
        unmask_irq(desc);
    }

    spinlock_unlock(&desc->lock);
}

int disable_irq(irq_hook_t* hook)
{
    struct irq_desc* desc = irq_to_desc(hook->irq);

    spinlock_lock(&desc->lock);

    if (desc->active_ids & hook->id) {
        spinlock_unlock(&desc->lock);
        return FALSE;
    }

    desc->active_ids |= hook->id;
    mask_irq(desc);

    spinlock_unlock(&desc->lock);
    return TRUE;
}

int irq_domain_set_hwirq_and_chip(struct irq_domain* domain, unsigned int virq,
                                  unsigned int hwirq,
                                  const struct irq_chip* chip, void* chip_data)
{
    struct irq_data* irq_data = irq_domain_get_irq_data(domain, virq);
    if (!irq_data) return -ENOENT;

    irq_data->hwirq = hwirq;
    irq_data->chip = (struct irq_chip*)(chip ? chip : &no_irq_chip);
    irq_data->chip_data = chip_data;

    return 0;
}

void irq_domain_set_info(struct irq_domain* domain, unsigned int virq,
                         unsigned int hwirq, const struct irq_chip* chip,
                         void* chip_data, irq_flow_handler_t handler)
{
    irq_domain_set_hwirq_and_chip(domain, virq, hwirq, chip, chip_data);
    irq_set_handler(virq, handler, FALSE);
}

static struct irq_domain* alloc_irq_domain(unsigned int fwid)
{
    int i;

    spinlock_lock(&irq_domain_lock);

    for (i = 0; i < NR_IRQ_DOMAIN; i++) {
        struct irq_domain* domain = &irq_domain[i];

        if (domain->fwid == IRQ_DOMAIN_NONE) {
            domain->fwid = fwid;

            spinlock_unlock(&irq_domain_lock);
            return domain;
        }
    }

    spinlock_unlock(&irq_domain_lock);
    return NULL;
}

struct irq_domain* irq_domain_add(unsigned int fwid, unsigned int hwirq_max,
                                  const struct irq_domain_ops* ops,
                                  void* host_data)
{
    struct irq_domain* domain;

    if (hwirq_max > NR_HWIRQ) return NULL;

    domain = alloc_irq_domain(fwid);
    if (!domain) return NULL;

    domain->ops = ops;
    domain->host_data = host_data;
    domain->hwirq_max = hwirq_max;

    return domain;
}

struct irq_domain* irq_find_matching_fwspec(struct irq_fwspec* fwspec)
{
    struct irq_domain* found = NULL;
    int i;

    spinlock_lock(&irq_domain_lock);

    for (i = 0; i < NR_IRQ_DOMAIN; i++) {
        struct irq_domain* domain = &irq_domain[i];
        int match;

        match = domain->fwid == fwspec->fwid;

        if (match) {
            found = domain;
            break;
        }
    }

    spinlock_unlock(&irq_domain_lock);
    return found;
}

struct irq_data* irq_domain_get_irq_data(struct irq_domain* domain,
                                         unsigned int virq)
{
    struct irq_data* irq_data;

    for (irq_data = irq_get_irq_data(virq); irq_data;
         irq_data = irq_data->parent_data)
        if (irq_data->domain == domain) return irq_data;

    return NULL;
}

struct irq_desc* irq_resolve_mapping(struct irq_domain* domain,
                                     unsigned int hwirq, unsigned int* irq)
{
    struct irq_data* data;
    struct irq_desc* desc = NULL;

    if (domain == NULL) return 0;

    spinlock_lock(&domain->revmap_lock);

    data = domain->revmap[hwirq];
    if (data) {
        desc = irq_data_to_desc(data);
        if (irq) *irq = data->irq;
    }

    spinlock_unlock(&domain->revmap_lock);

    return desc;
}

unsigned int irq_find_mapping(struct irq_domain* domain, unsigned int hwirq)
{
    unsigned int irq;

    if (irq_resolve_mapping(domain, hwirq, &irq)) return irq;

    return 0;
}

static int irq_alloc_descs(int irq, unsigned int from, unsigned int cnt)
{
    int i, start, ret;

    if (!cnt) return -EINVAL;

    if (irq >= 0) {
        if (from > irq) return -EINVAL;
        from = irq;
    }

    spinlock_lock(&irq_allocate_lock);

    start = bitmap_find_next_zero_area(allocated_irqs, NR_IRQ, from, cnt);

    ret = -EEXIST;
    if (irq >= 0 && start != irq) goto unlock;
    ret = -ENOMEM;
    if (start + cnt > NR_IRQ) goto unlock;

    for (i = 0; i < cnt; i++)
        SET_BIT(allocated_irqs, start + i);
    ret = start;

unlock:
    spinlock_unlock(&irq_allocate_lock);
    return ret;
}

int irq_domain_alloc_descs(int virq, unsigned int cnt, unsigned int hwirq)
{
    unsigned int hint;

    if (virq >= 0) {
        virq = irq_alloc_descs(virq, virq, cnt);
    } else {
        hint = hwirq % NR_IRQ;
        if (!hint) hint++;

        virq = irq_alloc_descs(-1, hint, cnt);
        if (virq <= 0 && hint > 1) {
            virq = irq_alloc_descs(-1, 1, cnt);
        }
    }

    return virq;
}

static void irq_free_descs(unsigned int virq, unsigned int cnt) {}

static int irq_domain_alloc_irq_data(struct irq_domain* domain,
                                     unsigned int virq, unsigned int nr_irqs)
{
    struct irq_data* irq_data;
    int i;

    for (i = 0; i < nr_irqs; i++) {
        irq_data = irq_get_irq_data(virq + i);
        irq_data->domain = domain;
    }

    return 0;
}

static void irq_domain_free_irq_data(unsigned int virq, unsigned int nr_irqs) {}

int __irq_domain_alloc_irqs(struct irq_domain* domain, int irq_base,
                            unsigned int nr_irqs, void* arg)
{
    int i, ret, virq;

    if (!domain) return -EINVAL;

    virq = irq_domain_alloc_descs(irq_base, nr_irqs, 0);
    if (virq < 0) return virq;

    if (irq_domain_alloc_irq_data(domain, virq, nr_irqs)) {
        ret = -ENOMEM;
        goto out_free_desc;
    }

    spinlock_lock(&irq_domain_lock);
    if (!domain->ops->alloc)
        ret = -ENOSYS;
    else
        ret = domain->ops->alloc(domain, virq, nr_irqs, arg);

    if (ret < 0) {
        spinlock_unlock(&irq_domain_lock);
        goto out_free_irq_data;
    }

    spinlock_lock(&domain->revmap_lock);
    for (i = 0; i < nr_irqs; i++) {
        struct irq_data* data = irq_get_irq_data(virq + i);
        domain->revmap[data->hwirq] = data;
    }
    spinlock_unlock(&domain->revmap_lock);

    spinlock_unlock(&irq_domain_lock);
    return virq;

out_free_irq_data:
    irq_domain_free_irq_data(virq, nr_irqs);
out_free_desc:
    irq_free_descs(virq, nr_irqs);
    return ret;
}

int irq_domain_translate_onecell(struct irq_domain* d,
                                 struct irq_fwspec* fwspec,
                                 unsigned int* out_hwirq,
                                 unsigned int* out_type)
{
    if (fwspec->param_count < 1) return -EINVAL;
    *out_hwirq = fwspec->param[0];
    *out_type = IRQ_TYPE_NONE;
    return 0;
}

int irq_domain_translate_twocell(struct irq_domain* d,
                                 struct irq_fwspec* fwspec,
                                 unsigned int* out_hwirq,
                                 unsigned int* out_type)
{
    if (fwspec->param_count < 2) return -EINVAL;
    *out_hwirq = fwspec->param[0];
    *out_type = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;
    return 0;
}

int irq_domain_translate_onetwocell(struct irq_domain* d,
                                    struct irq_fwspec* fwspec,
                                    unsigned int* out_hwirq,
                                    unsigned int* out_type)
{
    if (fwspec->param_count < 1) return -EINVAL;
    *out_hwirq = fwspec->param[0];
    if (fwspec->param_count > 1)
        *out_type = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;
    else
        *out_type = IRQ_TYPE_NONE;
    return 0;
}

static int irq_domain_translate(struct irq_domain* d, struct irq_fwspec* fwspec,
                                unsigned int* hwirq, unsigned int* type)
{
    if (d->ops->translate) return d->ops->translate(d, fwspec, hwirq, type);

    *hwirq = fwspec->param[0];
    return 0;
}

unsigned int irq_create_fwspec_mapping(struct irq_fwspec* fwspec)
{
    struct irq_domain* domain;
    unsigned int hwirq;
    unsigned int type = IRQ_TYPE_NONE;
    int virq;

    domain = irq_find_matching_fwspec(fwspec);
    if (!domain) return 0;

    if (irq_domain_translate(domain, fwspec, &hwirq, &type)) return 0;

    virq = irq_find_mapping(domain, hwirq);
    if (virq) {
        return virq;
    }

    virq = irq_domain_alloc_irqs(domain, 1, fwspec);
    if (virq <= 0) return 0;

    return virq;
}

#if CONFIG_OF
unsigned int irq_create_of_mapping(struct of_phandle_args* oirq)
{
    struct irq_fwspec fwspec;

    if (oirq->args_count > IRQ_DOMAIN_IRQ_SPEC_PARAMS) return 0;

    memset(&fwspec, 0, sizeof(fwspec));
    fwspec.fwid = oirq->phandle;
    fwspec.param_count = oirq->args_count;
    memcpy(fwspec.param, oirq->args, sizeof(u32) * oirq->args_count);

    return irq_create_fwspec_mapping(&fwspec);
}
#endif
