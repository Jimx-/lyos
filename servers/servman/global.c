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

#define _SERVMAN_GLOBAL_VARIABLE_HERE_

#include "lyos/config.h"
#include "lyos/type.h"
#include <sys/types.h>  
#include "lyos/list.h"
#include <lyos/const.h>
#include "global.h"

PUBLIC  struct sproc sproc_table[NR_PRIV_PROCS];
PUBLIC  struct sproc * sproc_ptr[NR_PROCS];

PUBLIC struct boot_priv boot_priv_table[] = {
    {TASK_MM,       "MM"        , TASK_FLAGS },
    {TASK_PM,       "PM"        , TASK_FLAGS },
    {TASK_SERVMAN,  "SERVMAN"   , TASK_FLAGS },
    {TASK_DEVMAN,   "DEVMAN"    , TASK_FLAGS },
    {TASK_SCHED,    "SCHED"     , TASK_FLAGS },
    {TASK_FS,       "VFS"       , TASK_FLAGS },
    {TASK_SYS,      "SYS"       , TASK_FLAGS },
    {TASK_TTY,      "TTY"       , TASK_FLAGS },
    {TASK_RD,       "RD"        , TASK_FLAGS },
    {TASK_INITFS,   "INITFS"    , TASK_FLAGS },
    {TASK_SYSFS,    "SYSFS"     , TASK_FLAGS },
    {INIT,          "INIT"      , USER_FLAGS },
    {NO_TASK,       ""          , 0          },
};
