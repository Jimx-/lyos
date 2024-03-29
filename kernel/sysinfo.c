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

#include <lyos/ipc.h>
#include "lyos/const.h"
#include <lyos/log.h>
#include <lyos/param.h>

struct sysinfo sysinfo __attribute__((section(".usermapped")));

kinfo_t kinfo __attribute__((section(".usermapped")));
struct kern_log kern_log __attribute__((section(".usermapped")));

struct machine machine __attribute__((section(".usermapped")));

struct kclockinfo kclockinfo __attribute__((section(".usermapped")));
