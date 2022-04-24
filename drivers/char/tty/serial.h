#ifndef _TTY_SERIAL_H_
#define _TTY_SERIAL_H_

#include <lyos/list.h>

#define UART_IBUFSIZE 1024 /* UART input buffer size */
#define UART_IBUFLOW  (UART_IBUFSIZE / 4)
#define UART_IBUFHIGH (UART_IBUFSIZE * 3 / 4)
#define UART_OBUFSIZE 1024 /* UART output buffer size */

struct uart_port;

struct uart_port_ops {
    void (*start_rx)(struct uart_port*);
    void (*stop_rx)(struct uart_port*);
    void (*start_tx)(struct uart_port*);
    void (*set_termios)(struct uart_port*, struct termios*);
    void (*startup)(struct uart_port*);
    void (*handle_irq)(struct uart_port*);
};

struct uart_port {
    struct list_head list;

    char *ihead, *itail;
    char *ohead, *otail;

    int icount, ocount;
    int idevready;

    char ibuf[UART_IBUFSIZE];
    char obuf[UART_OBUFSIZE];

    int irq;
    int irq_hook_id;

    const struct uart_port_ops* ops;
};

void arch_init_serial(void);

int init_uart(TTY* tty);
void uart_interrupt(MESSAGE* m);

void uart_add_port(struct uart_port* uport, const struct uart_port_ops* ops);

void uart_insert_char(struct uart_port* uport, unsigned int ch);

#endif
