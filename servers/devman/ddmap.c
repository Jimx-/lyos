/*  
    (c)Copyright 2011 Jimx
    
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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/list.h"
#include "lyos/driver.h"
#include "global.h"
#include <stdlib.h>
#include <lyos/sysutils.h>

PUBLIC void init_dd_map()
{
    int i;
    for (i = 0; i < MAJOR_MAX; i++) {
        INIT_LIST_HEAD(&dd_map[i]);
    }
}

PUBLIC int map_driver(dev_t dev, int type, endpoint_t drv_ep)
{
    struct dev_driver_map * map = (struct dev_driver_map *)malloc(sizeof(struct dev_driver_map));
    if (map == NULL) return ENOMEM;

    map->minor = MINOR(dev);
    map->type = type;

    map->drv_ep = drv_ep;

    list_add(&(map->list), &dd_map[MAJOR(dev)]);

    return 0;
}

PUBLIC int do_announce_driver(MESSAGE * m)
{
    int type = m->FLAGS;
    endpoint_t drv_ep = m->source;
    dev_t dev = m->DEVICE;
    dev_t major = MAJOR(dev), minor = MINOR(dev);

    char * name = (char *)malloc(m->NAME_LEN + 1);
    if (name == NULL) return ENOMEM;
    data_copy(getpid(), name, m->source, m->BUF, m->NAME_LEN);
    name[m->NAME_LEN] = '\0';

    //printl("DEVMAN: Registering driver #%d for /dev/%s (%c%d,%d)\n", drv_ep, name, type == DT_BLOCKDEV ? 'b' : 'c', 
    //                major, minor);

    int retval = map_driver(dev, type, drv_ep);
    if (retval) return retval;

    return 0;
}

PUBLIC int do_get_driver(MESSAGE * m)
{
    dev_t dev = m->DEVICE;
    dev_t major = MAJOR(dev), minor = MINOR(dev);
    int type = m->FLAGS;

    struct dev_driver_map * map;
    list_for_each_entry(map, &dd_map[major], list) {
        if (map->minor == minor && map->type == type) {
            m->PID = map->drv_ep;
            return 0;
        }
    }
    return ENXIO;
}

