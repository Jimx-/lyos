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
#include "lyos/config.h"
#include <lyos/endpoint.h>
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include <asm/page.h>

#include "region.h"
#include "proto.h"
#include "global.h"

int mm_verify_endpt(endpoint_t ep, int* proc_nr)
{
    *proc_nr = ENDPOINT_P(ep);
    return 0;
}

struct mmproc* endpt_mmproc(endpoint_t ep)
{
    int proc_nr;
    int r = mm_verify_endpt(ep, &proc_nr);
    if (r) return NULL;

    return &mmproc_table[proc_nr];
}
