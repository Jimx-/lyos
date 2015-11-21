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
#include <sys/types.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include "arch.h"

PRIVATE void omap3_disp_char(const char c)
{
    char* a = (char*) uart_base_addr;
    *a = c;
}

MACHINE_START(OMAP3_BEAGLE, "OMAP3 Beagle Board")
    .uart_base = (phys_bytes) 0x49020000,
    .serial_putc = omap3_disp_char,
MACHINE_END
