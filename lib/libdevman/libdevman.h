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

PUBLIC bus_type_id_t bus_register(char* name);
PUBLIC device_id_t device_register(struct device_info* devinf);

#endif
