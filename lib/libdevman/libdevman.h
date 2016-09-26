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

#ifndef _LIBDEVMAN_H_
#define _LIBDEVMAN_H_

#include <lyos/list.h>
    
typedef int bus_type_id_t;
#define BUS_NAME_MAX    32
#define BUS_TYPE_ERROR  -1

typedef int device_id_t;
#define NO_DEVICE_ID    -1

#define DEVICE_NAME_MAX 32
struct device_info {
    char name[DEVICE_NAME_MAX];
    device_id_t parent;
    bus_type_id_t bus;
};

typedef int dev_attr_id_t;
#define ATTR_NAME_MAX   32
struct bus_attr_info {
    char name[ATTR_NAME_MAX];
    mode_t mode;
    bus_type_id_t bus;
};

struct bus_attribute;
typedef ssize_t (*bus_attr_show_t)(struct bus_attribute* attr,char* buf);
typedef ssize_t (*bus_attr_store_t)(struct bus_attribute* attr, const char* buf, size_t count);
struct bus_attribute {
    struct bus_attr_info info;
    dev_attr_id_t id;
    void* cb_data;
    struct list_head list;
    bus_attr_show_t show;
    bus_attr_store_t store;
};

PUBLIC bus_type_id_t bus_register(char* name);
PUBLIC device_id_t device_register(struct device_info* devinf);

PUBLIC int devman_init_bus_attr(struct bus_attribute* attr, bus_type_id_t bus, char* name, mode_t mode, void* cb_data,
                                bus_attr_show_t show, bus_attr_store_t store);
PUBLIC int devman_bus_attr_add(struct bus_attribute* attr);
PUBLIC ssize_t devman_bus_attr_handle(MESSAGE* msg);

#endif
