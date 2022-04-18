#ifndef _LYOS_IRQ_H_
#define _LYOS_IRQ_H_

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

extern irq_hook_t irq_hooks[];

/* interrupt.c */
void init_irq(void);
void irq_handle(int irq);
void put_irq_handler(int irq, irq_hook_t* hook, irq_handler_t handler);
void rm_irq_handler(irq_hook_t* hook);
int disable_irq(irq_hook_t* hook);
void enable_irq(irq_hook_t* hook);

int put_local_timer_handler(irq_handler_t handler);

#endif
