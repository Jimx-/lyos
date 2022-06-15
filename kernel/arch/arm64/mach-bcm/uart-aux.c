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
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/proto.h>

#include "serial.h"

#define UART_TX 0x40

#define UART_LSR      0x54
#define UART_LSR_TEMT 0x40
#define UART_LSR_THRE 0x20

void* uart_base_addr;

void uart_aux_disp_char(const char c)
{
    unsigned int status;

    if (!uart_base_addr) return;

    mmio_write(uart_base_addr + UART_TX, c);

    for (;;) {
        status = mmio_read(uart_base_addr + UART_LSR);
        if ((status & (UART_LSR_TEMT | UART_LSR_THRE)) ==
            (UART_LSR_TEMT | UART_LSR_THRE))
            break;
        arch_pause();
    }
}
