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
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include <errno.h>
#include "string.h"
#include "lyos/fs.h"
#include <lyos/irqctl.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/io.h>
#include <lyos/sysutils.h>

#include "tty.h"
#include "console.h"
#include "proto.h"
#include "global.h"
#include "serial.h"

#include <libfdt/libfdt.h>
#include <libof/libof.h>

/* 8250 constants */
#define UART_FREQ 115200L /* timer frequency */

/* Interrupt enable bits */
#define IE_RECEIVER_READY      1
#define IE_TRANSMITTER_READY   2
#define IE_LINE_STATUS_CHANGE  4
#define IE_MODEM_STATUS_CHANGE 8

/* Line control bits */
#define LC_CS5             0x00 /* LSB0 and LSB1 encoding for CS5 */
#define LC_CS6             0x01 /* LSB0 and LSB1 encoding for CS6 */
#define LC_CS7             0x02 /* LSB0 and LSB1 encoding for CS7 */
#define LC_CS8             0x03 /* LSB0 and LSB1 encoding for CS8 */
#define LC_2STOP_BITS      0x04
#define LC_PARITY          0x08
#define LC_PAREVEN         0x10
#define LC_BREAK           0x40
#define LC_ADDRESS_DIVISOR 0x80

/* Interrupt status bits */
#define IS_MODEM_STATUS_CHANGE 0
#define IS_NOTPENDING          1
#define IS_TRANSMITTER_READY   2
#define IS_RECEIVER_READY      4
#define IS_LINE_STATUS_CHANGE  6
#define IS_IDBITS              6

/* Line status bits */
#define LS_OVERRUN_ERR       2
#define LS_PARITY_ERR        4
#define LS_FRAMING_ERR       8
#define LS_BREAK_INTERRUPT   0x10
#define LS_TRANSMITTER_READY 0x20
#define LS_TRANSMITTER_EMPTY 0x40

/* Modem control bits */
#define MC_DTR  1
#define MC_RTS  2
#define MC_OUT2 8 /* required for PC & AT interrupts */

/* Modem status bits */
#define MS_CTS   0x10
#define MS_RLSD  0x80 /* Received Line Signal Detect */
#define MS_DRLSD 0x08 /* RLSD Delta */

struct rs232 {
    struct uart_port uport;

    int cts;

    unsigned char ostate;
#define ODONE     1
#define ORAW      2
#define OWAKEUP   4
#define ODEVREADY MS_CTS
#define OQUEUED   0x20
#define OSWREADY  0x40
#define ODEVHUP   MS_RLSD

    u32 ier;

    void* reg_base;
    void* xmit_port;
    void* recv_port;
    void* div_low_port;
    void* div_hi_port;
    void* int_enab_port;
    void* int_id_port;
    void* line_ctl_port;
    void* modem_ctl_port;
    void* line_status_port;
    void* modem_status_port;
};

#define uport_to_rs232(uport) list_entry((uport), struct rs232, uport)

#define devready(rs) ((readl((rs)->modem_status_port) | (rs)->cts) & MS_CTS)
#define txready(rs)                                     \
    ((readl((rs)->line_status_port) &                   \
      (LS_TRANSMITTER_READY | LS_TRANSMITTER_EMPTY)) == \
     (LS_TRANSMITTER_READY | LS_TRANSMITTER_EMPTY))

#define istart(rs) writel((rs)->modem_ctl_port, MC_OUT2 | MC_RTS | MC_DTR)
#define istop(rs)  writel((rs)->modem_ctl_port, MC_OUT2 | MC_DTR)

static const char* const bcm2835aux_compat[] = {"brcm,bcm2835-aux-uart", NULL};

static void rs_start_rx(struct uart_port* uport)
{
    struct rs232* rs = uport_to_rs232(uport);
    istart(rs);

    rs->ier |= IE_RECEIVER_READY;
    writel(rs->int_enab_port, rs->ier);
}

static void rs_stop_rx(struct uart_port* uport)
{
    struct rs232* rs = uport_to_rs232(uport);
    istop(rs);

    rs->ier &= ~IE_RECEIVER_READY;
    writel(rs->int_enab_port, rs->ier);
}

static void rs_set_termios(struct uart_port* uport, struct termios* termios)
{
    struct rs232* rs = uport_to_rs232(uport);

    static struct speed_divisor {
        int speed;
        int divisor;
    } sp2d[] = {
        {B50, UART_FREQ / 50},       {B75, UART_FREQ / 75},
        {B110, UART_FREQ / 110},     {B134, UART_FREQ * 10 / 1345},
        {B150, UART_FREQ / 150},     {B200, UART_FREQ / 200},
        {B300, UART_FREQ / 300},     {B600, UART_FREQ / 600},
        {B1200, UART_FREQ / 1200},   {B1800, UART_FREQ / 1800},
        {B2400, UART_FREQ / 2400},   {B4800, UART_FREQ / 4800},
        {B9600, UART_FREQ / 9600},   {B19200, UART_FREQ / 19200},
        {B38400, UART_FREQ / 38400},
        //{ B57600,   UART_FREQ / 57600   },
        //{ B115200,  UART_FREQ / 115200L },
    };

    struct speed_divisor* sd;

    rs->cts = (termios->c_cflag & CLOCAL) ? MS_CTS : 0;

    int divisor = 0;
    for (sd = sp2d; sd < sp2d + sizeof(sp2d) / sizeof(sp2d[0]); sd++) {
        if (sd->speed == termios->c_ospeed) divisor = sd->divisor;
    }
    if (divisor == 0) return;

    int line_controls = 0;
    if (termios->c_cflag & CPARENB) {
        line_controls |= LC_PARITY;
        if (!(termios->c_cflag & CPARODD)) line_controls |= LC_PAREVEN;
    }

    if (divisor >= (UART_FREQ / 110)) line_controls |= LC_2STOP_BITS;

    if ((termios->c_cflag & CSIZE) == CS5)
        line_controls |= LC_CS5;
    else if ((termios->c_cflag & CSIZE) == CS6)
        line_controls |= LC_CS6;
    else if ((termios->c_cflag & CSIZE) == CS7)
        line_controls |= LC_CS7;
    else if ((termios->c_cflag & CSIZE) == CS8)
        line_controls |= LC_CS8;

    /* writel(rs->line_ctl_port, LC_ADDRESS_DIVISOR); */
    /* writel(rs->div_low_port, divisor); */
    /* writel(rs->div_hi_port, divisor >> 8); */
    writel(rs->line_ctl_port, line_controls);
}

static void rs_startup(struct uart_port* uport)
{
    struct rs232* rs = uport_to_rs232(uport);

    irq_setpolicy(rs->uport.irq, 0, &rs->uport.irq_hook_id);
    irq_enable(&rs->uport.irq_hook_id);

    rs->ier = IE_LINE_STATUS_CHANGE | IE_MODEM_STATUS_CHANGE;
    writel(rs->int_enab_port, rs->ier);

    rs_start_rx(uport);

    rs->ostate = ODEVREADY | OSWREADY;
}

static void rs_in_int(struct rs232* rs)
{
    u32 ch;
    ch = readl(rs->recv_port);
    uart_insert_char(&rs->uport, ch);
}

static void rs_out_int(struct rs232* rs)
{
    while (txready(rs) && rs->ostate >= (ODEVREADY | OSWREADY | OQUEUED)) {
        writel(rs->xmit_port, *rs->uport.otail);

        if (++rs->uport.otail == bufend(rs->uport.obuf))
            rs->uport.otail = rs->uport.obuf;
        if (--rs->uport.ocount == 0) {
            rs->ostate &= ~OQUEUED;
        }
    }

    if (!(rs->ostate & OQUEUED)) {
        rs->ier &= ~IE_TRANSMITTER_READY;
        writel(rs->int_enab_port, rs->ier);
    }
}

static void rs_start_tx(struct uart_port* uport)
{
    struct rs232* rs = uport_to_rs232(uport);

    rs->ostate |= OQUEUED;
    if (txready(rs)) rs_out_int(rs);

    if (rs->ostate & OQUEUED) {
        rs->ier |= IE_TRANSMITTER_READY;
        writel(rs->int_enab_port, rs->ier);
    }
}

static void rs_handle_irq(struct uart_port* uport)
{
    struct rs232* rs = uport_to_rs232(uport);
    int i = 1000;

    while (i--) {
        int v;
        v = readl(rs->int_id_port);

        if (v & IS_NOTPENDING) break;
        switch (v & IS_IDBITS) {
        case IS_RECEIVER_READY:
            rs_in_int(rs);
            continue;
        case IS_TRANSMITTER_READY:
            rs_out_int(rs);
            continue;
        }

        break;
    }

    irq_enable(&uport->irq_hook_id);
}

static const struct uart_port_ops rs232_uart_ops = {
    .start_rx = rs_start_rx,
    .stop_rx = rs_stop_rx,
    .start_tx = rs_start_tx,
    .set_termios = rs_set_termios,
    .startup = rs_startup,
    .handle_irq = rs_handle_irq,
};

static int fdt_scan_uart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct rs232* rs;
    phys_bytes base, size;
    void* reg_base;
    int irq;
    int ret;

    if (!of_flat_dt_match(blob, offset, bcm2835aux_compat)) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    reg_base = mm_map_phys(SELF, base, size, MMP_IO);
    if (reg_base == MAP_FAILED) return 0;

    irq = irq_of_parse_and_map(blob, offset, 0);
    if (!irq) return 0;

    rs = malloc(sizeof(*rs));
    if (!rs) return 1;

    memset(rs, 0, sizeof(*rs));
    rs->reg_base = reg_base;
    rs->xmit_port = rs->reg_base + 0;
    rs->recv_port = rs->reg_base + 0;
    rs->div_low_port = rs->reg_base + 0;
    rs->div_hi_port = rs->reg_base + (1 << 2);
    rs->int_enab_port = rs->reg_base + (1 << 2);
    rs->int_id_port = rs->reg_base + (2 << 2);
    rs->line_ctl_port = rs->reg_base + (3 << 2);
    rs->modem_ctl_port = rs->reg_base + (4 << 2);
    rs->line_status_port = rs->reg_base + (5 << 2);
    rs->modem_status_port = rs->reg_base + (6 << 2);

    rs->uport.irq = irq;
    rs->uport.irq_hook_id = irq;
    uart_add_port(&rs->uport, &rs232_uart_ops);

    return 0;
}

void bcm2835aux_scan(void) { of_scan_fdt(fdt_scan_uart, NULL, boot_params); }
