#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>
#include <lyos/sysutils.h>

#include <libchardriver/libchardriver.h>

#include "tty.h"
#include "console.h"
#include "serial.h"
#include "proto.h"
#include "global.h"

static DEF_LIST(uart_ports);

static irq_id_t uart_irq_set;

static struct uart_port* uart_get_line(unsigned int line)
{
    struct uart_port* uport;

    list_for_each_entry(uport, &uart_ports, list)
    {
        if (!line) return uport;
        line--;
    }

    return NULL;
}

void uart_add_port(struct uart_port* uport, const struct uart_port_ops* ops)
{
    uport->ops = ops;
    list_add_tail(&uport->list, &uart_ports);
}

void uart_insert_char(struct uart_port* uport, unsigned int ch)
{
    if (uport->icount >= buflen(uport->ibuf)) return; /* buffer full */

    if (++uport->icount >= UART_IBUFHIGH && uport->idevready) {
        uport->ops->stop_rx(uport);
        uport->idevready = FALSE;
    }
    *uport->ihead = ch;
    if (++uport->ihead >= bufend(uport->ibuf)) uport->ihead = uport->ibuf;
}

static void uart_read(TTY* tty)
{
    int count, icount;
    struct uart_port* uport = (struct uart_port*)tty->tty_dev;

    while ((count = uport->icount) > 0) {
        icount = bufend(uport->ibuf) - uport->itail;
        count = min(count, icount);

        if ((count = in_process(tty, uport->itail, count)) == 0) break;

        uport->icount -= count;
        if (!uport->idevready && uport->icount < UART_IBUFLOW) {
            uport->ops->start_rx(uport);
            uport->idevready = TRUE;
        }
        if ((uport->itail += count) == bufend(uport->ibuf))
            uport->itail = uport->ibuf;
    }
}

static void uart_write(TTY* tty)
{
    struct uart_port* uport = (struct uart_port*)tty->tty_dev;
    int retval = 0;

    while (TRUE) {
        int ocount = buflen(uport->obuf) - uport->ocount;
        int count = bufend(uport->obuf) - uport->ohead;
        count = min(count, ocount);
        count = min(count, tty->tty_outleft);

        if (count == 0) break;

        if (tty->tty_outcaller == TASK_TTY)
            memcpy(uport->ohead, (char*)tty->tty_outbuf + tty->tty_outcnt,
                   count);
        else if ((retval = safecopy_from(tty->tty_outcaller, tty->tty_outgrant,
                                         tty->tty_outcnt, uport->ohead,
                                         count)) != OK) {
            retval = -retval;
            goto reply;
        }

        ocount = count;
        uport->ocount += ocount;
        uport->ops->start_tx(uport);
        if ((uport->ohead += ocount) >= bufend(uport->obuf))
            uport->ohead -= buflen(uport->obuf);

        tty->tty_outcnt += ocount;
        tty->tty_outleft -= count;

    reply:
        if (tty->tty_outleft == 0 || retval != OK) {
            if (tty->tty_outcaller != TASK_TTY) {
                chardriver_reply_io(tty->tty_outcaller, tty->tty_outid,
                                    tty->tty_outleft == 0 ? tty->tty_outcnt
                                                          : retval);
            }
            tty->tty_outcaller = NO_TASK;
            tty->tty_outcnt = 0;
        }
    }
}

static void uart_echo(TTY* tty, char c)
{
    struct uart_port* uport = (struct uart_port*)tty->tty_dev;
    int ocount;

    if (buflen(uport->obuf) == uport->ocount) return;
    ocount = 1;
    *uport->ohead = c;

    uport->ocount += ocount;
    uport->ops->start_tx(uport);

    if ((uport->ohead += ocount) >= bufend(uport->obuf))
        uport->ohead -= buflen(uport->obuf);
}

int init_uart(TTY* tty)
{
    int line = tty - &tty_table[NR_CONSOLES];
    struct uart_port* uport = uart_get_line(line);

    if (!uport) return -ENXIO;

    tty->tty_dev = uport;

    uport->ihead = uport->itail = uport->ibuf;
    uport->ohead = uport->otail = uport->obuf;
    uport->icount = uport->ocount = 0;

    uart_irq_set |= (irq_id_t)1UL << uport->irq;

    uport->ops->set_termios(uport, &tty->tty_termios);

    tty->tty_devread = uart_read;
    tty->tty_devwrite = uart_write;
    tty->tty_echo = uart_echo;

    uport->ops->startup(uport);
    return 0;
}

void uart_interrupt(MESSAGE* m)
{
    unsigned long irq_set = m->INTERRUPTS;
    struct uart_port* uport;

    if (!(irq_set & uart_irq_set)) return;

    list_for_each_entry(uport, &uart_ports, list)
    {
        if (irq_set & (1UL << uport->irq)) uport->ops->handle_irq(uport);
    }
}
