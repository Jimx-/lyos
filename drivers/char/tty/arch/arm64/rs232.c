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
#include "proto.h"
#include "global.h"

#include <libfdt/libfdt.h>
#include <libof/libof.h>

irq_id_t rs_irq_set;

int init_rs(TTY* tty) { return 0; }

int rs_interrupt(MESSAGE* m) { return 0; }
