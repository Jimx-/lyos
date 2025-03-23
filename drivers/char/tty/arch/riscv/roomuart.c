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

#define UART_REG_ENABLE 0x00
#define UART_REG_DATA   0x04
#define UART_REG_STATUS 0x08

#define UART_SR_TXEMPTY (1 << 0)
#define UART_SR_TXBUSY  (1 << 2)

struct roomuart_port {
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
};

#define uport_to_roomuart(uport) \
    list_entry((uport), struct roomuart_port, uport)

static const char* const roomuart_compat[] = {"roomsoc,uart0", NULL};

static unsigned int room_read(const struct roomuart_port* rs, unsigned int reg)
{
    void* addr = rs->reg_base + reg;
    return readl(addr);
}

static void room_write(const struct roomuart_port* rs, unsigned int reg,
                       unsigned int val)
{
    void* addr = rs->reg_base + reg;
    writel(addr, val);
}

static void room_start_rx(struct uart_port* uport) {}

static void room_stop_rx(struct uart_port* uport) {}

static void room_set_termios(struct uart_port* uport, struct termios* termios)
{}

static void room_startup(struct uart_port* uport)
{
    struct roomuart_port* rs = uport_to_roomuart(uport);

    irq_setpolicy(rs->uport.irq, IRQ_REENABLE, &rs->uport.irq_hook_id);
    irq_enable(&rs->uport.irq_hook_id);

    room_write(rs, UART_REG_ENABLE, 1);

    rs->ostate = ODEVREADY | OSWREADY;
}

static void room_out_int(struct roomuart_port* rs)
{
    while (rs->ostate >= (ODEVREADY | OSWREADY | OQUEUED)) {
        room_write(rs, UART_REG_DATA, *rs->uport.otail);

        while (room_read(rs, UART_REG_STATUS) & UART_SR_TXBUSY)
            ;

        if (++rs->uport.otail == bufend(rs->uport.obuf))
            rs->uport.otail = rs->uport.obuf;
        if (--rs->uport.ocount == 0) {
            rs->ostate &= ~OQUEUED;
        }
    }
}

static void room_start_tx(struct uart_port* uport)
{
    struct roomuart_port* rs = uport_to_roomuart(uport);

    rs->ostate |= OQUEUED;
    room_out_int(rs);
}

static void room_handle_irq(struct uart_port* uport) {}

static const struct uart_port_ops room_uart_ops = {
    .start_rx = room_start_rx,
    .stop_rx = room_stop_rx,
    .start_tx = room_start_tx,
    .set_termios = room_set_termios,
    .startup = room_startup,
    .handle_irq = room_handle_irq,
};

static int fdt_scan_uart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    struct roomuart_port* rs;
    phys_bytes base, size;
    void* reg_base;
    int irq;
    int ret;

    if (!of_flat_dt_match(blob, offset, roomuart_compat)) return 0;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return 0;

    reg_base = mm_map_phys(SELF, base, size, 0);
    if (reg_base == MAP_FAILED) return 0;

    irq = irq_of_parse_and_map(blob, offset, 0);
    if (!irq) return 0;

    rs = malloc(sizeof(*rs));
    if (!rs) return 1;

    memset(rs, 0, sizeof(*rs));
    rs->reg_base = reg_base;

    rs->uport.irq = irq;
    rs->uport.irq_hook_id = irq;
    uart_add_port(&rs->uport, &room_uart_ops);

    return 0;
}

void roomuart_scan(void) { of_scan_fdt(fdt_scan_uart, NULL, boot_params); }
