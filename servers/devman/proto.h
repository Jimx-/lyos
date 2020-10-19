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

#include <lyos/types.h>
#include <libdevman/libdevman.h>

#include "type.h"
#include "const.h"

/* ddmap.c */
void init_dd_map();
int map_driver(dev_t dev, int type, endpoint_t drv_ep);
int do_device_add(MESSAGE* m);
int do_get_driver(MESSAGE* m);

/* bus.c */
void init_bus();
void bus_domain_label(struct bus_type* bus, char* buf);
int do_bus_register(MESSAGE* m);
struct bus_type* get_bus_type(bus_type_id_t id);
int do_bus_attr_add(MESSAGE* m);

/* class.c */
void init_class();
void class_domain_label(struct class* bus, char* buf);
int class_register(const char* name, endpoint_t owner, class_id_t* id);
int do_class_register(MESSAGE* m);
struct class* get_class(class_id_t id);

/* device.c */
void init_device();
int do_device_register(MESSAGE* m);
struct device* get_device(device_id_t id);
char* device_get_path(struct device* dev);
int do_device_attr_add(MESSAGE* m);

/* uevent.c */
void uevent_init(void);
int device_uevent_env(struct device* dev, enum kobject_action action,
                      char* envp[]);
int device_uevent(struct device* dev, enum kobject_action action);
int device_synth_uevent(struct device* dev, const char* buf, size_t count);

#endif
