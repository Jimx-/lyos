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

#define _MM_GLOBAL_VARIABLE_HERE_

#include <sys/types.h>
#include "lyos/config.h"
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/list.h"
#include "global.h"

struct mmproc mmproc_table[NR_PROCS];
int pt_init_done = 0;
