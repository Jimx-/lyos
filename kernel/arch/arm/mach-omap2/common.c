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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include "arch.h"
#include "arch_proto.h"
#include "serial.h"

PUBLIC void* uart_base_addr;

PUBLIC void omap3_disp_char(const char c)
{
    int i;
    char* base = (char*) uart_base_addr;

    for (i = 0; i < 100000; i++) {
        if (mmio_read(base + OMAP3_LSR) & OMAP3_LSR_THRE) {
            break;
        }
    }

    mmio_write(base + OMAP3_THR, c);

    for (i = 0; i < 100000; i++) {
        if (mmio_read(base + OMAP3_LSR) & (OMAP3_LSR_THRE | OMAP3_LSR_TEMT)) {
            break;
        }
    }
}
