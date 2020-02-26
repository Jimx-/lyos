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

#ifndef _DEVMAN_PROTO_H_
#define _DEVMAN_PROTO_H_

#include <libdevman/libdevman.h>

PUBLIC void init_dd_map();
PUBLIC int map_driver(dev_t dev, int type, endpoint_t drv_ep);
PUBLIC int do_device_add(MESSAGE* m);
PUBLIC int do_get_driver(MESSAGE* m);

PUBLIC void init_bus();
PUBLIC bus_type_id_t do_bus_register(MESSAGE* m);
PUBLIC struct bus_type* get_bus_type(bus_type_id_t id);
PUBLIC int do_bus_attr_add(MESSAGE* m);

PUBLIC void init_device();
PUBLIC device_id_t do_device_register(MESSAGE* m);
PUBLIC struct device* get_device(device_id_t id);
PUBLIC int do_device_attr_add(MESSAGE* m);

#endif
