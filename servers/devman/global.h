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

#ifndef _DEVMAN_GLOBAL_H_
#define _DEVMAN_GLOBAL_H_

#include <lyos/types.h>
#include <lyos/list.h>

/* EXTERN is extern except for global.c */
#ifdef _DEVMAN_GLOBAL_VARIABLE_HERE_
#undef EXTERN
#define EXTERN
#endif

/* ddmap */
EXTERN struct list_head dd_map[MAJOR_MAX];

#endif
