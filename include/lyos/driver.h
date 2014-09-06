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

#ifndef _DRIVER_H_
#define _DRIVER_H_

#define DT_BLOCKDEV    1
#define DT_CHARDEV     2

#define MAJOR_MAX       64

/* The entry points of a device driver */
struct dev_driver{
    char * dev_name;
    int (*dev_open) (MESSAGE * p);
    int (*dev_close)  (MESSAGE * p);
    int (*dev_read)  (MESSAGE * p);
    int (*dev_write) (MESSAGE * p);
    int (*dev_ioctl)  (MESSAGE * p);
};

struct dev_driver_map{
    struct list_head list;

    dev_t minor;
    int type;

    endpoint_t drv_ep;
};

PUBLIC void dev_driver_task(struct dev_driver * dd);
PUBLIC int  rw_sector (int io_type, int dev, u64 pos,
            int bytes, int proc_nr, void * buf);
PUBLIC int announce_blockdev(char * name, dev_t dev);
PUBLIC int announce_chardev(char * name, dev_t dev);
PUBLIC endpoint_t get_blockdev_driver(dev_t dev);
PUBLIC endpoint_t get_chardev_driver(dev_t dev);

#endif

