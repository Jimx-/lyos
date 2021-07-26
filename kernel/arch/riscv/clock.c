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
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"

int arch_init_time() { return 0; }

int init_local_timer(int freq) { return 0; }

void setup_local_timer_one_shot(void) {}

void setup_local_timer_periodic(void) {}

void restart_local_timer() {}

void stop_local_timer() {}

int put_local_timer_handler(irq_handler_t handler) { return 0; }

void arch_stop_context(struct proc* p, u64 delta) {}

void get_cpu_ticks(unsigned int cpu, u64 ticks[CPU_STATES]) {}
