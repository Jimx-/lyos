#ifndef _LYOS_IRQ_H_
#define _LYOS_IRQ_H_

#include <lyos/spinlock.h>

struct irq_chip;
struct irq_domain;

enum {
    IRQ_TYPE_NONE = 0x00000000,
    IRQ_TYPE_EDGE_RISING = 0x00000001,
    IRQ_TYPE_EDGE_FALLING = 0x00000002,
    IRQ_TYPE_EDGE_BOTH = (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING),
    IRQ_TYPE_LEVEL_HIGH = 0x00000004,
    IRQ_TYPE_LEVEL_LOW = 0x00000008,
    IRQ_TYPE_LEVEL_MASK = (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH),
    IRQ_TYPE_SENSE_MASK = 0x0000000f,
    IRQ_TYPE_DEFAULT = IRQ_TYPE_SENSE_MASK,
};

#define IRQ_DOMAIN_IRQ_SPEC_PARAMS 16

struct irq_fwspec {
    unsigned int fwid;
    int param_count;
    u32 param[IRQ_DOMAIN_IRQ_SPEC_PARAMS];
};

struct irq_data {
    unsigned int irq;
    unsigned int hwirq;
    struct irq_chip* chip;
    struct irq_domain* domain;
    void* chip_data;
    struct irq_data* parent_data;
};

struct irq_chip {
    void (*irq_mask)(struct irq_data* data);
    void (*irq_unmask)(struct irq_data* data);
    void (*irq_eoi)(struct irq_data* data);
    void (*irq_used)(struct irq_data* data);
    void (*irq_not_used)(struct irq_data* data);
};

typedef struct irq_hook {
    int irq;
    irq_hook_id_t id;
    int (*handler)(struct irq_hook*);
    struct irq_hook* next;
    endpoint_t proc_ep;
    irq_hook_id_t notify_id;
    irq_policy_t irq_policy;
} irq_hook_t;

typedef int (*irq_handler_t)(irq_hook_t* irq_hook);

struct irq_domain_ops;

#define IRQ_DOMAIN_NONE ((unsigned int)-1)

struct irq_domain {
    unsigned int fwid;
    const struct irq_domain_ops* ops;
    void* host_data;
    unsigned int hwirq_max;
    spinlock_t revmap_lock;
    struct irq_data* revmap[NR_HWIRQ];
};

struct irq_domain_ops {
    int (*alloc)(struct irq_domain* d, unsigned int virq, unsigned int nr_irqs,
                 void* arg);
    int (*translate)(struct irq_domain* d, struct irq_fwspec* fwspec,
                     unsigned int* out_hwirq, unsigned int* out_type);
};

extern irq_hook_t irq_hooks[];

/* interrupt.c */
void init_irq(void);
void irq_handle(int irq);
void put_irq_handler(int irq, irq_hook_t* hook, irq_handler_t handler);
void rm_irq_handler(irq_hook_t* hook);
int disable_irq(irq_hook_t* hook);
void enable_irq(irq_hook_t* hook);

int put_local_timer_handler(irq_handler_t handler);

int irq_set_chip(unsigned int irq, const struct irq_chip* chip);

int irq_domain_set_hwirq_and_chip(struct irq_domain* domain, unsigned int virq,
                                  unsigned int hwirq,
                                  const struct irq_chip* chip, void* chip_data);

unsigned int irq_find_mapping(struct irq_domain* domain, unsigned int hwirq);

struct irq_data* irq_domain_get_irq_data(struct irq_domain* domain,
                                         unsigned int virq);
struct irq_domain* irq_domain_add(unsigned int fwid, unsigned int hwirq_max,
                                  const struct irq_domain_ops* ops,
                                  void* host_data);

int __irq_domain_alloc_irqs(struct irq_domain* domain, int irq_base,
                            unsigned int nr_irqs, void* arg);
static inline int irq_domain_alloc_irqs(struct irq_domain* domain,
                                        unsigned int nr_irqs, void* arg)
{
    return __irq_domain_alloc_irqs(domain, -1, nr_irqs, arg);
}

unsigned int irq_create_fwspec_mapping(struct irq_fwspec* fwspec);

int irq_domain_translate_onecell(struct irq_domain* d,
                                 struct irq_fwspec* fwspec,
                                 unsigned int* out_hwirq,
                                 unsigned int* out_type);
int irq_domain_translate_twocell(struct irq_domain* d,
                                 struct irq_fwspec* fwspec,
                                 unsigned int* out_hwirq,
                                 unsigned int* out_type);

void handle_domain_irq(struct irq_domain* domain, unsigned int hwirq);

#endif
