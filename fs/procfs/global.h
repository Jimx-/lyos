/*
    This file is part of Lyos.

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

#ifndef _PROCFS_GLOBAL_H_
#define _PROCFS_GLOBAL_H_

#include "type.h"
#include <kernel/proc.h>
#include "pm/pmproc.h"

extern struct procfs_file root_files[];
extern struct procfs_file pid_files[];

extern struct proc proc[NR_TASKS + NR_PROCS];
extern struct pmproc pmproc[NR_PROCS];

#endif
