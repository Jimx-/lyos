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
#define BUS_NAME_MAX 32
#define BUS_TYPE_ERROR -1

typedef int device_id_t;
#define NO_DEVICE_ID -1

#define DEVICE_NAME_MAX 32
struct device_info {
    char name[DEVICE_NAME_MAX];
    device_id_t parent;
    bus_type_id_t bus;
    dev_t devt;
    int type;
} __attribute__((packed));

typedef int dev_attr_id_t;
#define ATTR_NAME_MAX 32

struct bus_attr_info {
    char name[ATTR_NAME_MAX];
    mode_t mode;
    bus_type_id_t bus;
};

struct bus_attribute;
typedef ssize_t (*bus_attr_show_t)(struct bus_attribute* attr, char* buf);
typedef ssize_t (*bus_attr_store_t)(struct bus_attribute* attr, const char* buf,
                                    size_t count);
struct bus_attribute {
    struct bus_attr_info info;
    dev_attr_id_t id;
    void* cb_data;
    struct list_head list;
    bus_attr_show_t show;
    bus_attr_store_t store;
};

struct device_attr_info {
    char name[ATTR_NAME_MAX];
    mode_t mode;
    device_id_t device;
};

struct device_attribute;
typedef ssize_t (*device_attr_show_t)(struct device_attribute* attr, char* buf);
typedef ssize_t (*device_attr_store_t)(struct device_attribute* attr,
                                       const char* buf, size_t count);
struct device_attribute {
    struct device_attr_info info;
    dev_attr_id_t id;
    void* cb_data;
    struct list_head list;
    device_attr_show_t show;
    device_attr_store_t store;
};

#define DT_BLOCKDEV 1
#define DT_CHARDEV 2
PUBLIC int dm_bdev_add(dev_t dev);
PUBLIC int dm_cdev_add(dev_t dev);
PUBLIC endpoint_t dm_get_bdev_driver(dev_t dev);
PUBLIC endpoint_t dm_get_cdev_driver(dev_t dev);

PUBLIC bus_type_id_t dm_bus_register(char* name);
PUBLIC device_id_t dm_device_register(struct device_info* devinf);

PUBLIC int dm_init_bus_attr(struct bus_attribute* attr, bus_type_id_t bus,
                            char* name, mode_t mode, void* cb_data,
                            bus_attr_show_t show, bus_attr_store_t store);
PUBLIC int dm_bus_attr_add(struct bus_attribute* attr);
PUBLIC ssize_t dm_bus_attr_handle(MESSAGE* msg);

PUBLIC int dm_init_device_attr(struct device_attribute* attr,
                               device_id_t device, char* name, mode_t mode,
                               void* cb_data, device_attr_show_t show,
                               device_attr_store_t store);
PUBLIC int dm_device_attr_add(struct device_attribute* attr);
PUBLIC ssize_t dm_device_attr_handle(MESSAGE* msg);

#endif
