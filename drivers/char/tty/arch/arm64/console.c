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
#include "string.h"
#include "lyos/fs.h"
#include "tty.h"
#include "console.h"
#include <lyos/portio.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <libchardriver/libchardriver.h>
#include "proto.h"
#include "global.h"

static void dummy_write(TTY* tty)
{
    if (tty->tty_outleft == 0) return;

    tty->tty_outcnt += tty->tty_outleft;
    tty->tty_outleft = 0;

    if (tty->tty_outcaller != TASK_TTY)
        chardriver_reply_io(tty->tty_outcaller, tty->tty_outid,
                            tty->tty_outcnt);

    tty->tty_outcaller = NO_TASK;
    tty->tty_outcnt = 0;
}

void init_screen(TTY* tty) { tty->tty_devwrite = dummy_write; }

int is_current_console(CONSOLE* con) { return FALSE; }

void select_console(int nr_console) {}
