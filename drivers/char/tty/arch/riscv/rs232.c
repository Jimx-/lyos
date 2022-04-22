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
#include "tty.h"
#include "console.h"
#include <lyos/irqctl.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/io.h>
#include <lyos/sysutils.h>
#include "proto.h"
#include "global.h"

#include <libfdt/libfdt.h>
#include <libof/libof.h>
#include <libchardriver/libchardriver.h>

/* 8250 constants */
#define UART_FREQ 115200L /* timer frequency */

#define RS_IBUFSIZE 1024 /* RS232 input buffer size */
#define RS_IBUFLOW  (RS_IBUFSIZE / 4)
#define RS_IBUFHIGH (RS_IBUFSIZE * 3 / 4)
#define RS_OBUFSIZE 1024 /* RS232 output buffer size */

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

/* Modem control bits */
#define MC_DTR  1
#define MC_RTS  2
#define MC_OUT2 8 /* required for PC & AT interrupts */

/* Modem status bits */
#define MS_CTS   0x10
#define MS_RLSD  0x80 /* Received Line Signal Detect */
#define MS_DRLSD 0x08 /* RLSD Delta */

struct uart_scan_data {
    int line;
    uint64_t base, size;
    int interrupt;
};

typedef struct {
    TTY* rs_tty;

    int cts;

    unsigned char ostate;
#define ODONE     1
#define ORAW      2
#define OWAKEUP   4
#define ODEVREADY MS_CTS
#define OQUEUED   0x20
#define OSWREADY  0x40
#define ODEVHUP   MS_RLSD

    char *ihead, *itail;
    char *ohead, *otail;

    int icount, ocount;
    int idevready;

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

    int irq;
    int irq_hook_id;

    char ibuf[RS_IBUFSIZE];
    char obuf[RS_OBUFSIZE];
} rs232_t;

static rs232_t rs_lines[NR_SERIALS];

irq_id_t rs_irq_set;

#define devready(rs) ((readb((rs)->modem_status_port) | (rs)->cts) & MS_CTS)
#define txready(rs)  (readb((rs)->line_status_port) & LS_TRANSMITTER_READY)

#define istart(rs)                                            \
    (writeb((rs)->modem_ctl_port, MC_OUT2 | MC_RTS | MC_DTR), \
     (rs)->idevready = TRUE)
#define istop(rs) \
    (writeb((rs)->modem_ctl_port, MC_OUT2 | MC_DTR), (rs)->idevready = FALSE)

static int fdt_scan_uart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{

    struct uart_scan_data* usd = arg;
    phys_bytes base, size;
    int ret;

    if (!arg || strlen(name) < 4 || memcmp(name, "uart", 4) != 0) return 0;
    if (usd->line-- > 0) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    usd->interrupt = irq_of_parse_and_map(blob, offset, 0);
    if (!usd->interrupt) return 0;

    usd->base = base;
    usd->size = size;

    return 1;
}

static void rs_config(rs232_t* rs)
{
    TTY* tty = rs->rs_tty;
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

    rs->cts = (tty->tty_termios.c_cflag & CLOCAL) ? MS_CTS : 0;

    int divisor = 0;
    for (sd = sp2d; sd < sp2d + sizeof(sp2d) / sizeof(sp2d[0]); sd++) {
        if (sd->speed == tty->tty_termios.c_ospeed) divisor = sd->divisor;
    }
    if (divisor == 0) return;

    int line_controls = 0;
    if (tty->tty_termios.c_cflag & CPARENB) {
        line_controls |= LC_PARITY;
        if (!(tty->tty_termios.c_cflag & CPARODD)) line_controls |= LC_PAREVEN;
    }

    if (divisor >= (UART_FREQ / 110)) line_controls |= LC_2STOP_BITS;

    if ((tty->tty_termios.c_cflag & CSIZE) == CS5)
        line_controls |= LC_CS5;
    else if ((tty->tty_termios.c_cflag & CSIZE) == CS6)
        line_controls |= LC_CS6;
    else if ((tty->tty_termios.c_cflag & CSIZE) == CS7)
        line_controls |= LC_CS7;
    else if ((tty->tty_termios.c_cflag & CSIZE) == CS8)
        line_controls |= LC_CS8;

    writeb(rs->line_ctl_port, LC_ADDRESS_DIVISOR);
    writeb(rs->div_low_port, divisor);
    writeb(rs->div_hi_port, divisor >> 8);
    writeb(rs->line_ctl_port, line_controls);

    rs->ostate = devready(rs) | OSWREADY;
}

static void rs_read(TTY* tty)
{
    int count, icount;
    rs232_t* rs = (rs232_t*)tty->tty_dev;

    while ((count = rs->icount) > 0) {
        icount = bufend(rs->ibuf) - rs->itail;
        count = min(count, icount);

        if ((count = in_process(tty, rs->itail, count)) == 0) break;

        rs->icount -= count;
        if (!rs->idevready && rs->icount < RS_IBUFLOW) istart(rs);
        if ((rs->itail += count) == bufend(rs->ibuf)) rs->itail = rs->ibuf;
    }
}

static void rs_in_int(rs232_t* rs)
{
    u8 ch;
    ch = readb(rs->recv_port);

    if (rs->icount >= buflen(rs->ibuf)) return; /* buffer full */

    if (++rs->icount >= RS_IBUFHIGH && rs->idevready) istop(rs);
    *rs->ihead = ch;
    if (++rs->ihead >= bufend(rs->ibuf)) rs->ihead = rs->ibuf;
}

static void rs_out_int(rs232_t* rs)
{
    while (txready(rs) && rs->ostate >= (ODEVREADY | OSWREADY | OQUEUED)) {
        writeb(rs->xmit_port, *rs->otail);

        if (++rs->otail == bufend(rs->obuf)) rs->otail = rs->obuf;
        if (--rs->ocount == 0) {
            rs->ostate &= ~OQUEUED;
        }
    }
}

static void rs_ostart(rs232_t* rs)
{
    rs->ostate |= OQUEUED;
    if (txready(rs)) rs_out_int(rs);
}

static void rs_write(TTY* tty)
{
    rs232_t* rs = (rs232_t*)tty->tty_dev;
    int retval = 0;

    while (TRUE) {
        int ocount = buflen(rs->obuf) - rs->ocount;
        int count = bufend(rs->obuf) - rs->ohead;
        count = min(count, ocount);
        count = min(count, tty->tty_outleft);

        if (count == 0) break;

        if (tty->tty_outcaller == TASK_TTY)
            memcpy(rs->ohead, (char*)tty->tty_outbuf + tty->tty_outcnt, count);
        else if ((retval = safecopy_from(tty->tty_outcaller, tty->tty_outgrant,
                                         tty->tty_outcnt, rs->ohead, count)) !=
                 OK) {
            retval = -retval;
            goto reply;
        }

        ocount = count;
        rs->ocount += ocount;
        rs_ostart(rs);
        if ((rs->ohead += ocount) >= bufend(rs->obuf))
            rs->ohead -= buflen(rs->obuf);

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

static void rs_echo(TTY* tty, char c)
{
    rs232_t* rs = (rs232_t*)tty->tty_dev;
    int ocount;

    if (buflen(rs->obuf) == rs->ocount) return;
    ocount = 1;
    *rs->ohead = c;

    rs->ocount += ocount;
    rs_ostart(rs);
    if ((rs->ohead += ocount) >= bufend(rs->obuf))
        rs->ohead -= buflen(rs->obuf);
}

int init_rs(TTY* tty)
{
    int line = tty - &tty_table[NR_CONSOLES];
    rs232_t* rs;
    struct uart_scan_data usd;

    usd.base = 0;
    usd.line = line;
    of_scan_fdt(fdt_scan_uart, &usd, boot_params);

    if (!usd.base) return ENXIO;

    rs = tty->tty_dev = &rs_lines[line];
    rs->rs_tty = tty;

    rs->ihead = rs->itail = rs->ibuf;
    rs->ohead = rs->otail = rs->obuf;
    rs->icount = rs->ocount = 0;

    rs->reg_base = mm_map_phys(SELF, (void*)usd.base, usd.size);
    if (rs->reg_base == MAP_FAILED) return EFAULT;

    rs->xmit_port = rs->reg_base + 0;
    rs->recv_port = rs->reg_base + 0;
    rs->div_low_port = rs->reg_base + 0;
    rs->div_hi_port = rs->reg_base + 1;
    rs->int_enab_port = rs->reg_base + 1;
    rs->int_id_port = rs->reg_base + 2;
    rs->line_ctl_port = rs->reg_base + 3;
    rs->modem_ctl_port = rs->reg_base + 4;
    rs->line_status_port = rs->reg_base + 5;
    rs->modem_status_port = rs->reg_base + 6;

    istop(rs);
    rs_config(rs);
    rs->irq = usd.interrupt;
    rs->irq_hook_id = rs->irq;

    irq_setpolicy(rs->irq, IRQ_REENABLE, &rs->irq_hook_id);
    irq_enable(&rs->irq_hook_id);

    rs_irq_set = (1 << rs->irq);

    writeb(rs->int_enab_port, IE_LINE_STATUS_CHANGE | IE_MODEM_STATUS_CHANGE |
                                  IE_RECEIVER_READY | IE_TRANSMITTER_READY);

    tty->tty_devread = rs_read;
    tty->tty_devwrite = rs_write;
    tty->tty_echo = rs_echo;

    istart(rs);

    return 0;
}

static void rs_handle_irq(rs232_t* rs)
{
    int i = 1000;
    while (i--) {
        int v;
        v = readb(rs->int_id_port);

        if (v & IS_NOTPENDING) return;
        switch (v & IS_IDBITS) {
        case IS_RECEIVER_READY:
            rs_in_int(rs);
            continue;
        case IS_TRANSMITTER_READY:
            rs_out_int(rs);
            continue;
        }

        return;
    }
}

int rs_interrupt(MESSAGE* m)
{
    unsigned long irq_set = m->INTERRUPTS;
    rs232_t* rs = rs_lines;
    int i;

    for (i = 0; i < NR_SERIALS; i++, rs++) {
        if (irq_set & (1 << rs->irq)) rs_handle_irq(rs);
    }

    return 0;
}
