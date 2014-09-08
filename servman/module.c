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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include <fcntl.h>
#include "stddef.h"
#include "stdlib.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "proto.h"
#include <multiboot.h>

struct boot_module {
    endpoint_t ep;
    char * name;
};

PRIVATE struct boot_module boot_modules[] = {
    { TASK_DEVMAN, "DEVMAN" },
    { TASK_INITFS, "INITFS" },
    { -1, NULL }
};


PUBLIC int spawn_boot_modules()
{
    int retval;
    struct boot_module * bm;

    multiboot_module_t * mb_mod = (multiboot_module_t *)mb_mod_addr;
    mb_mod++;   /* skip initrd */

    for (bm = boot_modules; bm->name != NULL; bm++, mb_mod++) {
        if (mb_mod->pad) {
            printl("SERVMAN: Invalid boot module parameter(pad is not zero)");
        }

        char * mod_base = (char*)(mb_mod->mod_start + KERNEL_VMA);
        int mod_len = mb_mod->mod_end - mb_mod->mod_start;
        printl("SERVMAN: Spawning boot module %s, base: 0x%x, len: %d bytes\n", bm->name, mod_base, mod_len);
        
        retval = serv_spawn_module(bm->ep, mod_base, mod_len);
        if (retval) return retval;

        /* YOU ARE GO */
        proc_table[bm->ep].state = 0;
    }

    return 0;
}
