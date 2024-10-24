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
#define NO_BUS_ID    -1

typedef int class_id_t;
#define CLASS_NAME_MAX 32
#define NO_CLASS_ID    -1

typedef int device_id_t;
#define DEVICE_NAME_MAX 32
#define NO_DEVICE_ID    -1

struct device_info {
    char name[DEVICE_NAME_MAX];
    char path[DEVICE_NAME_MAX];
    device_id_t parent;
    bus_type_id_t bus;
    class_id_t class;
    dev_t devt;
    int type;
};

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
typedef ssize_t (*device_attr_show_t)(struct device_attribute* attr, char* buf,
                                      size_t size);
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
#define DT_CHARDEV  2
int dm_bdev_add(dev_t dev);
int dm_cdev_add(dev_t dev);
endpoint_t dm_get_bdev_driver(dev_t dev);
endpoint_t dm_get_cdev_driver(dev_t dev);

int dm_bus_register(const char* name, bus_type_id_t* id);
int dm_bus_get_or_create(const char* name, bus_type_id_t* idp);
int dm_class_register(const char* name, class_id_t* id);
int dm_class_get_or_create(const char* name, class_id_t* idp);
int dm_device_register(struct device_info* devinf, device_id_t* id);

int dm_init_bus_attr(struct bus_attribute* attr, bus_type_id_t bus, char* name,
                     mode_t mode, void* cb_data, bus_attr_show_t show,
                     bus_attr_store_t store);
int dm_bus_attr_add(struct bus_attribute* attr);
ssize_t dm_bus_attr_handle(MESSAGE* msg);

int dm_init_device_attr(struct device_attribute* attr, device_id_t device,
                        char* name, mode_t mode, void* cb_data,
                        device_attr_show_t show, device_attr_store_t store);
int dm_device_attr_add(struct device_attribute* attr);
void dm_device_attr_handle(MESSAGE* msg);

int dm_async_cdev_add(dev_t dev);
int dm_async_device_register(struct device_info* devinf, device_id_t* id);
int dm_async_device_attr_add(struct device_attribute* attr);

void dm_async_reply(MESSAGE* msg);

#endif
