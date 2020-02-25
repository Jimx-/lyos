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

#ifndef _DEVMAN_TYPE_H_
#define _DEVMAN_TYPE_H_

struct bus_type {
    bus_type_id_t id;
    char name[BUS_NAME_MAX];
    endpoint_t owner;
};

struct device {
    device_id_t id;
    struct device* parent;
    char name[DEVICE_NAME_MAX];
    endpoint_t owner;
    dev_t devt;
    int type;

    struct bus_type* bus;
};

#endif
