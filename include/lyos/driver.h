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

#define NR_DEVICES      135

#define MAJOR_MAX       64

PUBLIC void bdev_init();
PUBLIC int bdev_driver(dev_t dev);
PUBLIC ssize_t bdev_readwrite(int io_type, int dev, u64 pos,
            int bytes, int proc_nr, void * buf);

#endif

