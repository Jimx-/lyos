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

#include "tty.h"
#include "console.h"
#include "proto.h"
#include "global.h"
#include "serial.h"

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#define UART01x_DR         0x00  /* Data read or written from the interface. */
#define UART01x_RSR        0x04  /* Receive status register (Read). */
#define UART01x_ECR        0x04  /* Error clear register (Write). */
#define UART010_LCRH       0x08  /* Line control register, high byte. */
#define ST_UART011_DMAWM   0x08  /* DMA watermark configure register. */
#define UART010_LCRM       0x0C  /* Line control register, middle byte. */
#define ST_UART011_TIMEOUT 0x0C  /* Timeout period register. */
#define UART010_LCRL       0x10  /* Line control register, low byte. */
#define UART010_CR         0x14  /* Control register. */
#define UART01x_FR         0x18  /* Flag register (Read only). */
#define UART010_IIR        0x1C  /* Interrupt identification register (Read). */
#define UART010_ICR        0x1C  /* Interrupt clear register (Write). */
#define ST_UART011_LCRH_RX 0x1C  /* Rx line control register. */
#define UART01x_ILPR       0x20  /* IrDA low power counter register. */
#define UART011_IBRD       0x24  /* Integer baud rate divisor register. */
#define UART011_FBRD       0x28  /* Fractional baud rate divisor register. */
#define UART011_LCRH       0x2c  /* Line control register. */
#define ST_UART011_LCRH_TX 0x2c  /* Tx Line control register. */
#define UART011_CR         0x30  /* Control register. */
#define UART011_IFLS       0x34  /* Interrupt fifo level select. */
#define UART011_IMSC       0x38  /* Interrupt mask. */
#define UART011_RIS        0x3c  /* Raw interrupt status. */
#define UART011_MIS        0x40  /* Masked interrupt status. */
#define UART011_ICR        0x44  /* Interrupt clear register. */
#define UART011_DMACR      0x48  /* DMA control register. */
#define ST_UART011_XFCR    0x50  /* XON/XOFF control register. */
#define ST_UART011_XON1    0x54  /* XON1 register. */
#define ST_UART011_XON2    0x58  /* XON2 register. */
#define ST_UART011_XOFF1   0x5C  /* XON1 register. */
#define ST_UART011_XOFF2   0x60  /* XON2 register. */
#define ST_UART011_ITCR    0x80  /* Integration test control register. */
#define ST_UART011_ITIP    0x84  /* Integration test input register. */
#define ST_UART011_ABCR    0x100 /* Autobaud control register. */
#define ST_UART011_ABIMSC  0x15C /* Autobaud interrupt mask/clear register. */

#define UART011_FR_RI   0x100
#define UART011_FR_TXFE 0x080
#define UART011_FR_RXFF 0x040
#define UART01x_FR_TXFF 0x020
#define UART01x_FR_RXFE 0x010
#define UART01x_FR_BUSY 0x008
#define UART01x_FR_DCD  0x004
#define UART01x_FR_DSR  0x002
#define UART01x_FR_CTS  0x001
#define UART01x_FR_TMSK (UART01x_FR_TXFF + UART01x_FR_BUSY)

#define UART011_CR_CTSEN      0x8000 /* CTS hardware flow control */
#define UART011_CR_RTSEN      0x4000 /* RTS hardware flow control */
#define UART011_CR_OUT2       0x2000 /* OUT2 */
#define UART011_CR_OUT1       0x1000 /* OUT1 */
#define UART011_CR_RTS        0x0800 /* RTS */
#define UART011_CR_DTR        0x0400 /* DTR */
#define UART011_CR_RXE        0x0200 /* receive enable */
#define UART011_CR_TXE        0x0100 /* transmit enable */
#define UART011_CR_LBE        0x0080 /* loopback enable */
#define UART010_CR_RTIE       0x0040
#define UART010_CR_TIE        0x0020
#define UART010_CR_RIE        0x0010
#define UART010_CR_MSIE       0x0008
#define ST_UART011_CR_OVSFACT 0x0008 /* Oversampling factor */
#define UART01x_CR_IIRLP      0x0004 /* SIR low power mode */
#define UART01x_CR_SIREN      0x0002 /* SIR enable */
#define UART01x_CR_UARTEN     0x0001 /* UART enable */

#define UART011_OEIM   (1 << 10) /* overrun error interrupt mask */
#define UART011_BEIM   (1 << 9)  /* break error interrupt mask */
#define UART011_PEIM   (1 << 8)  /* parity error interrupt mask */
#define UART011_FEIM   (1 << 7)  /* framing error interrupt mask */
#define UART011_RTIM   (1 << 6)  /* receive timeout interrupt mask */
#define UART011_TXIM   (1 << 5)  /* transmit interrupt mask */
#define UART011_RXIM   (1 << 4)  /* receive interrupt mask */
#define UART011_DSRMIM (1 << 3)  /* DSR interrupt mask */
#define UART011_DCDMIM (1 << 2)  /* DCD interrupt mask */
#define UART011_CTSMIM (1 << 1)  /* CTS interrupt mask */
#define UART011_RIMIM  (1 << 0)  /* RI interrupt mask */

#define UART011_OEIS   (1 << 10) /* overrun error interrupt status */
#define UART011_BEIS   (1 << 9)  /* break error interrupt status */
#define UART011_PEIS   (1 << 8)  /* parity error interrupt status */
#define UART011_FEIS   (1 << 7)  /* framing error interrupt status */
#define UART011_RTIS   (1 << 6)  /* receive timeout interrupt status */
#define UART011_TXIS   (1 << 5)  /* transmit interrupt status */
#define UART011_RXIS   (1 << 4)  /* receive interrupt status */
#define UART011_DSRMIS (1 << 3)  /* DSR interrupt status */
#define UART011_DCDMIS (1 << 2)  /* DCD interrupt status */
#define UART011_CTSMIS (1 << 1)  /* CTS interrupt status */
#define UART011_RIMIS  (1 << 0)  /* RI interrupt status */

#define UART011_OEIC   (1 << 10) /* overrun error interrupt clear */
#define UART011_BEIC   (1 << 9)  /* break error interrupt clear */
#define UART011_PEIC   (1 << 8)  /* parity error interrupt clear */
#define UART011_FEIC   (1 << 7)  /* framing error interrupt clear */
#define UART011_RTIC   (1 << 6)  /* receive timeout interrupt clear */
#define UART011_TXIC   (1 << 5)  /* transmit interrupt clear */
#define UART011_RXIC   (1 << 4)  /* receive interrupt clear */
#define UART011_DSRMIC (1 << 3)  /* DSR interrupt clear */
#define UART011_DCDMIC (1 << 2)  /* DCD interrupt clear */
#define UART011_CTSMIC (1 << 1)  /* CTS interrupt clear */
#define UART011_RIMIC  (1 << 0)  /* RI interrupt clear */

struct uart_amba_port {
    struct uart_port uport;

    unsigned char ostate;
#define ODONE     1
#define ORAW      2
#define OWAKEUP   4
#define ODEVREADY 0x10
#define OQUEUED   0x20
#define OSWREADY  0x40
#define ODEVHUP   0x80

    void* reg_base;

    unsigned int im;
};

#define uport_to_uap(uport) list_entry((uport), struct uart_amba_port, uport)

static const char* const pl011_compat[] = {"brcm,bcm2835-pl011", "arm,pl011",
                                           NULL};

static unsigned int pl011_read(const struct uart_amba_port* uap,
                               unsigned int reg)
{
    void* addr = uap->reg_base + reg;
    return readl(addr);
}

static void pl011_write(const struct uart_amba_port* uap, unsigned int reg,
                        unsigned int val)
{
    void* addr = uap->reg_base + reg;
    writel(addr, val);
}

static void pl011_start_rx(struct uart_port* uport)
{
    struct uart_amba_port* uap = uport_to_uap(uport);
    unsigned int cr;

    cr = pl011_read(uap, UART011_CR);
    cr |= UART011_CR_RXE;
    pl011_write(uap, UART011_CR, cr);
}

static void pl011_stop_rx(struct uart_port* uport)
{
    struct uart_amba_port* uap = uport_to_uap(uport);
    unsigned int cr;

    cr = pl011_read(uap, UART011_CR);
    cr |= UART011_CR_RXE;
    pl011_write(uap, UART011_CR, cr);
}

static void pl011_set_termios(struct uart_port* uport, struct termios* termios)
{
    struct uart_amba_port* uap = uport_to_uap(uport);
}

static void pl011_startup(struct uart_port* uport)
{
    struct uart_amba_port* uap = uport_to_uap(uport);
    unsigned int cr;

    pl011_write(uap, UART011_ICR,
                UART011_OEIS | UART011_BEIS | UART011_PEIS | UART011_FEIS |
                    UART011_RTIS | UART011_RXIS);

    irq_setpolicy(uap->uport.irq, 0, &uap->uport.irq_hook_id);
    irq_enable(&uap->uport.irq_hook_id);

    uap->im = UART011_RXIM | UART011_RTIM;
    pl011_write(uap, UART011_IMSC, uap->im);

    cr = pl011_read(uap, UART011_CR);
    cr = UART011_CR_RTS | UART011_CR_DTR;
    cr |= UART01x_CR_UARTEN | UART011_CR_RXE | UART011_CR_TXE;
    pl011_write(uap, UART011_CR, cr);

    uap->ostate = ODEVREADY | OSWREADY;
}

static void pl011_in_int(struct uart_amba_port* uap)
{
    unsigned int status, ch;
    int i;

    status = pl011_read(uap, UART01x_FR);
    if (status & UART01x_FR_RXFE) return;

    ch = pl011_read(uap, UART01x_DR);
    uart_insert_char(&uap->uport, ch);
}

static int pl011_tx_char(struct uart_amba_port* uap, unsigned char c)
{
    if (pl011_read(uap, UART01x_FR) & UART01x_FR_TXFF) return FALSE;

    pl011_write(uap, UART01x_DR, c);

    return TRUE;
}

static void pl011_tx_chars(struct uart_amba_port* uap)
{
    while ((uap->ostate >= (ODEVREADY | OSWREADY | OQUEUED)) &&
           pl011_tx_char(uap, *uap->uport.otail)) {
        if (++uap->uport.otail == bufend(uap->uport.obuf))
            uap->uport.otail = uap->uport.obuf;
        if (--uap->uport.ocount == 0) {
            uap->ostate &= ~OQUEUED;
            uap->im &= ~UART011_TXIM;
            pl011_write(uap, UART011_IMSC, uap->im);
        }
    }
}

static void pl011_start_tx(struct uart_port* uport)
{
    struct uart_amba_port* uap = uport_to_uap(uport);

    uap->ostate |= OQUEUED;
    pl011_tx_chars(uap);

    if (uap->ostate & OQUEUED) {
        uap->im |= UART011_TXIM;
        pl011_write(uap, UART011_IMSC, uap->im);
    }
}

static void pl011_handle_irq(struct uart_port* uport)
{
    struct uart_amba_port* uap = uport_to_uap(uport);
    unsigned int status, counter = 256;

    status = pl011_read(uap, UART011_RIS) & uap->im;
    while (status) {
        pl011_write(uap, UART011_ICR,
                    status & ~(UART011_TXIS | UART011_RTIS | UART011_RXIS));

        if (status & (UART011_RTIS | UART011_RXIS)) pl011_in_int(uap);

        if (status & UART011_TXIS) pl011_tx_chars(uap);

        if (counter-- == 0) break;

        status = pl011_read(uap, UART011_RIS) & uap->im;
    }

    irq_enable(&uport->irq_hook_id);
}

static const struct uart_port_ops amba_pl011_ops = {
    .start_rx = pl011_start_rx,
    .stop_rx = pl011_stop_rx,
    .start_tx = pl011_start_tx,
    .set_termios = pl011_set_termios,
    .startup = pl011_startup,
    .handle_irq = pl011_handle_irq,
};

static int fdt_scan_uart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct uart_amba_port* uap;
    phys_bytes base, size;
    void* reg_base;
    int irq;
    int ret;

    if (!of_flat_dt_match(blob, offset, pl011_compat)) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    reg_base = mm_map_phys(SELF, base, size, MMP_IO);
    if (reg_base == MAP_FAILED) return 0;

    irq = irq_of_parse_and_map(blob, offset, 0);
    if (!irq) return 0;

    uap = malloc(sizeof(*uap));
    if (!uap) return 1;

    memset(uap, 0, sizeof(*uap));
    uap->reg_base = reg_base;

    uap->uport.irq = irq;
    uap->uport.irq_hook_id = irq;
    uart_add_port(&uap->uport, &amba_pl011_ops);

    return 0;
}

void arch_init_serial(void) { of_scan_fdt(fdt_scan_uart, NULL, boot_params); }
