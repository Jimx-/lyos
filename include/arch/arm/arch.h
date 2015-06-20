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

#ifndef _ARCH_ARCH_H_
#define _ARCH_ARCH_H_

#include "mach_type.h"

struct machine_desc {
    int id;
    const char* name;

    void (*map_io)(void);
};

#define MACHINE_START(_type,_name)          \
static const struct machine_desc __mach_desc_##_type    \
 __attribute__ ((used))    \
 __attribute__((__section__(".arch.info.init"))) = {    \
    .id     = MACH_TYPE_##_type,        \
    .name   = _name,

#define MACHINE_END             \
};

#endif
    