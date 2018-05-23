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

#define _TTY_GLOBAL_VARIABLE_HERE_

#include "lyos/config.h"
#include <lyos/type.h>
#include <lyos/ipc.h>
#include <lyos/ipc.h>
#include "lyos/list.h"
#include "tty.h"
#include "console.h"
#include <lyos/const.h>
#include "global.h"

PUBLIC  TTY     tty_table[NR_CONSOLES + NR_SERIALS];
PUBLIC  CONSOLE console_table[NR_CONSOLES];
